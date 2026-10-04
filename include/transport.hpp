#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace md {

class IMarketDataTransport {
public:
    virtual ~IMarketDataTransport() = default;
    virtual bool open(const std::string& host, std::uint16_t port,
                      int receive_buffer_bytes, std::string& error) = 0;
    virtual int wait_for_data(int timeout_ms) = 0;
    virtual std::ptrdiff_t receive(std::span<std::uint8_t> buffer) noexcept = 0;
    virtual void close() noexcept = 0;
};

class UdpTransport final : public IMarketDataTransport {
public:
    UdpTransport() = default;
    ~UdpTransport() override;

    bool open(const std::string& host, std::uint16_t port,
              int receive_buffer_bytes, std::string& error) override;
    int wait_for_data(int timeout_ms) override;
    std::ptrdiff_t receive(std::span<std::uint8_t> buffer) noexcept override;
    void close() noexcept override;

private:
    int socket_fd_{-1};
    int epoll_fd_{-1};
};

} // namespace md
