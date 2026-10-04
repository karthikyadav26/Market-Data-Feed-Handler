#include "csv_feed.hpp"
#include "encoder.hpp"
#include "timestamp.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sys/socket.h>

namespace {

struct Args {
    std::uint16_t port{9000};
    std::string destination{"127.0.0.1"};
    std::string input{};
    std::uint64_t count{10000};
    std::uint64_t seed{14};
    std::string rate{"max"};
    std::vector<std::uint32_t> instruments{1, 2};
    std::uint64_t gap_at{0};
    std::uint64_t malformed_every{0};
    std::size_t batch{1};
};

void usage(const char* name) {
    std::cout << "Usage: " << name << " [options]\n"
              << "  --count N                 number of generated messages\n"
              << "  --input PATH              read source CSV instead of generating\n"
              << "  --destination IP          UDP destination (default 127.0.0.1)\n"
              << "  --port N                  UDP destination port (default 9000)\n"
              << "  --seed N                  deterministic random seed (default 14)\n"
              << "  --instruments 1,2,3       instrument IDs (default 1,2)\n"
              << "  --rate max|N              messages per second\n"
              << "  --gap-at N                make generated message N jump one sequence\n"
              << "  --malformed-every N       truncate every Nth generated message\n"
              << "  --batch N                 messages per UDP datagram (default 1)\n"
              << "  --help                    show help\n";
}

bool u64(const char* s, std::uint64_t& v) {
    char* end = nullptr; errno = 0; auto x = std::strtoull(s, &end, 10);
    if (errno || end == s || *end) {
        return false;
    }

    v = static_cast<std::uint64_t>(x);
    return true;
}

bool split_instruments(const std::string& s, std::vector<std::uint32_t>& out) {
    out.clear(); std::size_t start = 0;
    while (start < s.size()) {
        auto end = s.find(',', start); if (end == std::string::npos) end = s.size();
        std::uint64_t v = 0; if (!u64(s.substr(start, end-start).c_str(), v) || v == 0 || v > 0xffffffffULL) return false;
        out.push_back(static_cast<std::uint32_t>(v)); start = end + 1;
    }
    return !out.empty();
}

bool args_parse(int argc, char** argv, Args& a) {
    for (int i=1;i<argc;++i) {
        std::string key=argv[i];
        auto next=[&](const char* k)->const char* { if(i+1>=argc){std::cerr<<k<<" needs a value\n";std::exit(2);} return argv[++i]; };
        if(key=="--help"){usage(argv[0]);std::exit(0);}
        if(key=="--count"){if(!u64(next("--count"),a.count))return false;}
        else if(key=="--input")a.input=next("--input");
        else if(key=="--destination")a.destination=next("--destination");
        else if(key=="--port"){std::uint64_t v;if(!u64(next("--port"),v)||v>65535)return false;a.port=static_cast<std::uint16_t>(v);}
        else if(key=="--seed"){if(!u64(next("--seed"),a.seed))return false;}
        else if(key=="--instruments"){if(!split_instruments(next("--instruments"),a.instruments))return false;}
        else if(key=="--rate")a.rate=next("--rate");
        else if(key=="--gap-at"){if(!u64(next("--gap-at"),a.gap_at))return false;}
        else if(key=="--malformed-every"){if(!u64(next("--malformed-every"),a.malformed_every))return false;}
        else if(key=="--batch"){std::uint64_t v;if(!u64(next("--batch"),v)||v==0||v>1000)return false;a.batch=static_cast<std::size_t>(v);}
        else { std::cerr << "unknown argument: " << key << '\n'; return false; }
    }
    if(a.rate!="max"){std::uint64_t ignored;if(!u64(a.rate.c_str(),ignored)||ignored==0)return false;}
    return true;
}

md::MarketDataMessage generate_message(std::uint64_t index, std::uint64_t seed,
                                       const std::vector<std::uint32_t>& instruments) {
    std::mt19937_64 rng(seed + index * 0x9e3779b97f4a7c15ULL);
    const std::uint64_t cycle = index / 5;
    const auto slot = static_cast<unsigned>(index % 5);
    const auto instrument = instruments[static_cast<std::size_t>(cycle % instruments.size())];
    const std::uint64_t base_order = 1'000'000ULL + cycle * 2ULL;
    md::MarketDataMessage msg{};
    msg.protocol_version = md::kProtocolVersion;
    msg.sequence_number = index + 1;
    msg.source_timestamp_ns = 1'000'000'000ULL + index * 1'000ULL;
    msg.instrument_id = instrument;
    if (slot == 0) {
        msg.type = md::MessageType::ADD_ORDER; msg.order_id = base_order + 1; msg.side = md::Side::BUY;
        msg.price_ticks = 10'000 + static_cast<std::int64_t>(rng() % 300); msg.quantity = 100;
    } else if (slot == 1) {
        msg.type = md::MessageType::ADD_ORDER; msg.order_id = base_order + 2; msg.side = md::Side::SELL;
        msg.price_ticks = 10'300 + static_cast<std::int64_t>(rng() % 300); msg.quantity = 50;
    } else if (slot == 2) {
        msg.type = md::MessageType::REPLACE_ORDER; msg.order_id = base_order + 1; msg.side = md::Side::BUY;
        msg.price_ticks = 10'050 + static_cast<std::int64_t>(rng() % 300); msg.quantity = 120;
    } else if (slot == 3) {
        msg.type = md::MessageType::EXECUTE_ORDER; msg.order_id = base_order + 1; msg.side = md::Side::BUY;
        msg.price_ticks = 10'000; msg.quantity = 20;
    } else {
        msg.type = md::MessageType::CANCEL_ORDER; msg.order_id = base_order + 2; msg.side = md::Side::SELL;
        msg.price_ticks = 10'300; msg.quantity = 50;
    }
    return msg;
}

} // namespace

int main(int argc, char** argv) {
    Args a;
    if(!args_parse(argc,argv,a)){usage(argv[0]);return 2;}
    const int fd=::socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0){std::cerr<<"socket: "<<std::strerror(errno)<<'\n';return 1;}
    sockaddr_in dst{}; dst.sin_family=AF_INET; dst.sin_port=htons(a.port);
    if(::inet_pton(AF_INET,a.destination.c_str(),&dst.sin_addr)!=1){std::cerr<<"invalid destination IP\n";::close(fd);return 2;}

    md::CsvFeedReader csv;
    if(!a.input.empty()){
        std::string err; if(!csv.open(a.input,err)){std::cerr<<err<<'\n';::close(fd);return 1;}
    }
    std::uint64_t sent=0; std::uint64_t next_sequence=1;
    const std::uint64_t rate = a.rate=="max" ? 0 : std::strtoull(a.rate.c_str(),nullptr,10);
    const auto period = rate ? std::chrono::duration<double>(1.0/static_cast<double>(rate)) : std::chrono::duration<double>(0.0);
    auto next_send=std::chrono::steady_clock::now();

    std::vector<std::uint8_t> datagram(a.batch*md::kWireMessageSize);
    bool input_eof=false;
    while(!input_eof && (a.input.empty() ? sent < a.count : true)) {
        std::size_t batch_count=0;
        while(batch_count<a.batch) {
            if(a.input.empty()) {
                if(sent>=a.count) break;
            } else if(input_eof) {
                break;
            }
            md::MarketDataMessage msg{}; std::string err;
            if(a.input.empty()) {
                msg=generate_message(sent,a.seed,a.instruments);
            } else {
                if(!csv.next(msg,err)) {
                    if(!err.empty()){ std::cerr<<err<<'\n'; ::close(fd); return 1; }
                    input_eof=true;
                    break;
                }
            }
            if(!a.input.empty()) msg.sequence_number = next_sequence;
            if(a.input.empty() && a.gap_at && sent+1==a.gap_at) msg.sequence_number += 1;
            next_sequence=msg.sequence_number+1;
            std::size_t written=0;
            if(!md::Encoder::encode(msg, std::span<std::uint8_t>(datagram.data()+batch_count*md::kWireMessageSize, md::kWireMessageSize), written)){
                std::cerr<<"encode failed for message "<<(sent+1)<<'\n';::close(fd);return 1;
            }
            ++batch_count; ++sent;
            if(rate) { next_send += std::chrono::duration_cast<std::chrono::steady_clock::duration>(period); std::this_thread::sleep_until(next_send); }
        }
        if(batch_count==0) continue;
        if(a.input.empty() && a.malformed_every) {
            // A malformed fixture is emitted as a truncated datagram for the Nth message.
            const std::size_t last_index = sent;
            if((last_index % a.malformed_every)==0) {
                const std::size_t full_without_last = (batch_count-1)*md::kWireMessageSize;
                const auto bytes = ::sendto(fd,datagram.data(),full_without_last+24,0,
                    reinterpret_cast<const sockaddr*>(&dst),sizeof(dst));
                if(bytes<0){std::cerr<<"sendto: "<<std::strerror(errno)<<'\n';::close(fd);return 1;}
                continue;
            }
        }
        const auto bytes = ::sendto(fd,datagram.data(),batch_count*md::kWireMessageSize,0,
            reinterpret_cast<const sockaddr*>(&dst),sizeof(dst));
        if(bytes<0){std::cerr<<"sendto: "<<std::strerror(errno)<<'\n';::close(fd);return 1;}
    }
    ::close(fd);
    std::cout<<"sent_messages="<<sent<<"\n";
    return 0;
}
