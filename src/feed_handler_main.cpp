#include "csv_output.hpp"
#include "pipeline.hpp"
#include "spsc_queue.hpp"
#include "timestamp.hpp"
#include "transport.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_stop{false};

void on_signal(int) noexcept { g_stop.store(true, std::memory_order_relaxed); }

struct Args {
    std::uint16_t port{9000};
    std::string bind_ip{"127.0.0.1"};
    std::string mode{"pipelined"};
    std::string record_path{};
    std::string output_path{};
    std::string latency_path{};
    std::size_t depth{1};
    std::int64_t count{0};
    int receive_buffer{4 * 1024 * 1024};
    std::uint64_t initial_seq{1};
};

void usage(const char* name) {
    std::cout << "Usage: " << name << " [options]\n"
              << "  --port N                 UDP port (default 9000)\n"
              << "  --bind IP                bind IPv4 address (default 127.0.0.1)\n"
              << "  --mode single|pipelined  processing mode (default pipelined)\n"
              << "  --record PATH            write capture file\n"
              << "  --output PATH            write top-of-book CSV\n"
              << "  --latency-output PATH    write latency CSV\n"
              << "  --depth 1|5|10           output top-N depth (1 uses snapshot format)\n"
              << "  --count N                stop after N decoded UDP messages\n"
              << "  --rcvbuf BYTES           SO_RCVBUF size (default 4194304)\n"
              << "  --initial-seq N          expected first sequence (default 1)\n"
              << "  --help                   show help\n";
}

bool parse_u64(const char* s, std::uint64_t& out) {
    char* end = nullptr;
    errno = 0;
    const auto v = std::strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    out = static_cast<std::uint64_t>(v);
    return true;
}

bool parse_i64(const char* s, std::int64_t& out) {
    char* end = nullptr;
    errno = 0;
    const auto v = std::strtoll(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return false;
    out = static_cast<std::int64_t>(v);
    return true;
}

bool parse_args(int argc, char** argv, Args& args) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--help") { usage(argv[0]); std::exit(0); }
        auto next = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << flag << " requires a value\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") {
            std::uint64_t v; if (!parse_u64(next("--port"), v) || v > 65535) return false; args.port = static_cast<std::uint16_t>(v);
        } else if (a == "--bind") args.bind_ip = next("--bind");
        else if (a == "--mode") args.mode = next("--mode");
        else if (a == "--record") args.record_path = next("--record");
        else if (a == "--output") args.output_path = next("--output");
        else if (a == "--latency-output") args.latency_path = next("--latency-output");
        else if (a == "--depth") { std::uint64_t v; if (!parse_u64(next("--depth"), v) || (v != 1 && v != 5 && v != 10)) return false; args.depth = static_cast<std::size_t>(v); }
        else if (a == "--count") { if (!parse_i64(next("--count"), args.count) || args.count < 0) return false; }
        else if (a == "--rcvbuf") { std::int64_t v; if (!parse_i64(next("--rcvbuf"), v) || v <= 0 || v > std::numeric_limits<int>::max()) return false; args.receive_buffer = static_cast<int>(v); }
        else if (a == "--initial-seq") { if (!parse_u64(next("--initial-seq"), args.initial_seq)) return false; }
        else { std::cerr << "unknown argument: " << a << "\n"; return false; }
    }
    if (args.mode != "single" && args.mode != "pipelined") return false;
    return true;
}

void print_metrics(const md::FeedMetrics& metrics, const md::SequenceValidator& seq) {
    std::cout << "messages_received=" << metrics.messages_received.load() << '\n'
              << "messages_decoded=" << metrics.messages_decoded.load() << '\n'
              << "messages_dropped=" << metrics.messages_dropped.load() << '\n'
              << "gaps_detected=" << metrics.gaps_detected.load() << '\n'
              << "duplicates_detected=" << metrics.duplicates_detected.load() << '\n'
              << "out_of_order_messages=" << metrics.out_of_order_messages.load() << '\n'
              << "decode_errors=" << metrics.decode_errors.load() << '\n'
              << "queue_drops=" << metrics.queue_drops.load() << '\n'
              << "book_updates_failed=" << metrics.book_updates_failed.load() << '\n'
              << "next_expected_sequence=" << seq.expected() << '\n';
}

} // namespace



int main(int argc, char** argv) {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    Args args;
    if (!parse_args(argc, argv, args)) {
        usage(argv[0]);
        return 2;
    }

    md::UdpTransport transport;
    std::string error;
    if (!transport.open(args.bind_ip, args.port, args.receive_buffer, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    md::FeedMetrics metrics;
    md::SequenceValidator sequence(args.initial_seq);
    md::LocalOrderBook book;
    md::FeedRecorder recorder;
    if (!args.record_path.empty() && !recorder.open(args.record_path, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    md::SnapshotWriter writer;
    md::SnapshotWriter* writer_ptr = nullptr;
    if (!args.output_path.empty()) {
        if (!writer.open(args.output_path, args.depth, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        writer_ptr = &writer;
    }
    if (!args.latency_path.empty()) {
        if (!writer.open_latency(args.latency_path, error)) {
            std::cerr << error << '\n';
            return 1;
        }
    }

    if (args.mode == "single") {
        md::FeedProcessor processor(book, sequence, metrics,
                                     args.record_path.empty() ? nullptr : &recorder,
                                     writer_ptr);
        std::array<std::uint8_t, md::kMaxDatagramSize> datagram{};
        std::uint64_t processed = 0;
        while (!g_stop.load(std::memory_order_relaxed)) {
            const int ready = transport.wait_for_data(250);
            if (ready < 0) {
                std::cerr << "epoll_wait failed\n";
                break;
            }
            if (ready == 0) continue;
            while (true) {
                const auto receive_ts = md::monotonic_now_ns();
                const auto n = transport.receive(std::span<std::uint8_t>(datagram.data(), datagram.size()));
                if (n <= 0) break;
                std::size_t ignored_accepted = 0;
                processor.handle_raw(std::span<const std::uint8_t>(datagram.data(), static_cast<std::size_t>(n)),
                                     receive_ts, ignored_accepted);
                processed = metrics.messages_decoded.load(std::memory_order_relaxed);
                if (args.count > 0 && static_cast<std::int64_t>(processed) >= args.count) {
                    g_stop.store(true, std::memory_order_relaxed);
                    break;
                }
            }
        }
        writer.close();
        recorder.close();
        transport.close();
        print_metrics(metrics, sequence);
        return 0;
    }

    md::SpscQueue<md::FeedEvent, 4096> queue;
    std::atomic<bool> receiver_done{false};
    std::atomic<std::int64_t> decoded_count{0};

    std::thread receiver([&] {
        std::array<std::uint8_t, md::kMaxDatagramSize> datagram{};
        while (!g_stop.load(std::memory_order_relaxed)) {
            const int ready = transport.wait_for_data(100);
            if (ready < 0) break;
            if (ready == 0) continue;
            while (!g_stop.load(std::memory_order_relaxed)) {
                const auto receive_ts = md::monotonic_now_ns();
                const auto n = transport.receive(std::span<std::uint8_t>(datagram.data(), datagram.size()));
                if (n <= 0) break;
                std::size_t cursor = 0;
                while (cursor < static_cast<std::size_t>(n) && !g_stop.load(std::memory_order_relaxed)) {
                    const std::size_t remaining = static_cast<std::size_t>(n) - cursor;
                    const std::size_t chunk = remaining < md::kWireMessageSize ? remaining : md::kWireMessageSize;
                    metrics.messages_received.fetch_add(1, std::memory_order_relaxed);
                    md::MarketDataMessage msg{};
                    md::DecodeError decode_error = md::DecodeError::NONE;
                    if (!md::Decoder::decode(std::span<const std::uint8_t>(datagram.data() + cursor, chunk), msg, decode_error)) {
                        metrics.decode_errors.fetch_add(1, std::memory_order_relaxed);
                        metrics.messages_dropped.fetch_add(1, std::memory_order_relaxed);
                        cursor = static_cast<std::size_t>(n);
                        break;
                    }
                    metrics.messages_decoded.fetch_add(1, std::memory_order_relaxed);
                    const auto decode_timestamp_ns = md::monotonic_now_ns();
                    const auto seq = sequence.observe(msg.sequence_number);
                    if (seq == md::SequenceEvent::SEQUENCE_GAP) metrics.gaps_detected.fetch_add(1, std::memory_order_relaxed);
                    if (seq == md::SequenceEvent::DUPLICATE) metrics.duplicates_detected.fetch_add(1, std::memory_order_relaxed);
                    if (seq == md::SequenceEvent::OUT_OF_ORDER) metrics.out_of_order_messages.fetch_add(1, std::memory_order_relaxed);
                    if (!args.record_path.empty()) recorder.record(receive_ts,
                        std::span<const std::uint8_t>(datagram.data() + cursor, md::kWireMessageSize));
                    if (seq == md::SequenceEvent::DUPLICATE || seq == md::SequenceEvent::OUT_OF_ORDER) {
                        metrics.messages_dropped.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        md::FeedEvent ev{msg, receive_ts, decode_timestamp_ns};
                        bool queued = queue.try_push(ev);
                        while (!queued && !g_stop.load(std::memory_order_relaxed)) {
                            std::this_thread::yield();
                            queued = queue.try_push(ev);
                        }
                        if (queued) decoded_count.fetch_add(1, std::memory_order_relaxed);
                    }
                    cursor += md::kWireMessageSize;
                    if (args.count > 0 && decoded_count.load(std::memory_order_relaxed) >= args.count) {
                        g_stop.store(true, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        }
        receiver_done.store(true, std::memory_order_release);
    });

    md::FeedProcessor book_processor(book, sequence, metrics,
                                     nullptr, writer_ptr);
    // The receiver owns sequence validation in pipelined mode; this processor only applies events.
    md::FeedEvent event{};
    while (!receiver_done.load(std::memory_order_acquire) || !queue.empty()) {
        if (queue.try_pop(event)) {
            (void)book_processor.handle_event(event);
        } else {
            std::this_thread::yield();
        }
    }
    if (receiver.joinable()) receiver.join();
    writer.close();
    recorder.close();
    transport.close();
    print_metrics(metrics, sequence);
    return 0;
}
