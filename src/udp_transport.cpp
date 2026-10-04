#include "transport.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace md {

UdpTransport::~UdpTransport() {
    close();
}

bool UdpTransport::open(const std::string& host, std::uint16_t port,
                        int receive_buffer_bytes, std::string& error) {
    close();
    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        error = std::string("socket: ") + std::strerror(errno);
        return false;
    }

    int flags = ::fcntl(socket_fd_, F_GETFL, 0);
    if (flags < 0 || ::fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        error = std::string("fcntl(O_NONBLOCK): ") + std::strerror(errno);
        close();
        return false;
    }

    int rcvbuf = receive_buffer_bytes;
    if (rcvbuf > 0 && ::setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF,
                                   &rcvbuf, sizeof(rcvbuf)) < 0) {
        error = std::string("setsockopt(SO_RCVBUF): ") + std::strerror(errno);
        close();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        error = "invalid IPv4 address: " + host;
        close();
        return false;
    }
    if (::bind(socket_fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        error = std::string("bind: ") + std::strerror(errno);
        close();
        return false;
    }

    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0) {
        error = std::string("epoll_create1: ") + std::strerror(errno);
        close();
        return false;
    }

    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    ev.data.fd = socket_fd_;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_fd_, &ev) < 0) {
        error = std::string("epoll_ctl(ADD): ") + std::strerror(errno);
        close();
        return false;
    }
    return true;
}

int UdpTransport::wait_for_data(int timeout_ms) {
    if (epoll_fd_ < 0) return -1;
    epoll_event ev{};
    return ::epoll_wait(epoll_fd_, &ev, 1, timeout_ms);
}

std::ptrdiff_t UdpTransport::receive(std::span<std::uint8_t> buffer) noexcept {
    if (socket_fd_ < 0) return -1;
    const ssize_t n = ::recv(socket_fd_, buffer.data(), buffer.size(), 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    return static_cast<std::ptrdiff_t>(n);
}

void UdpTransport::close() noexcept {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

} // namespace md
