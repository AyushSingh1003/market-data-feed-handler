#pragma once

#include "protocol.h"
#include <sys/socket.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <errno.h>

namespace mdfh {

class MarketDataSocket {
 public:
  MarketDataSocket() : sock_fd_(-1), connected_(false) {}
  ~MarketDataSocket() { disconnect(); }

  bool connect(const std::string& host, uint16_t port, uint32_t timeout_ms = 5000) {
    sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd_ < 0) {
      std::cerr << "Failed to create socket\n";
      return false;
    }
    set_tcp_nodelay(true);
    set_recv_buffer_size(4 * 1024 * 1024);
    set_send_buffer_size(4 * 1024 * 1024);
    set_socket_priority(6);
    int flags = fcntl(sock_fd_, F_GETFL, 0);
    fcntl(sock_fd_, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
    int ret = ::connect(sock_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret < 0) {
      if (errno == EINPROGRESS) {
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(sock_fd_, &write_fds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ret = select(sock_fd_ + 1, nullptr, &write_fds, nullptr, &tv);
        if (ret <= 0) {
          std::cerr << "Connection timeout\n";
          close(sock_fd_);
          sock_fd_ = -1;
          return false;
        }
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(sock_fd_, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
          std::cerr << "Connection failed: " << strerror(error) << "\n";
          close(sock_fd_);
          sock_fd_ = -1;
          return false;
        }
      } else {
        std::cerr << "Connection failed immediately\n";
        close(sock_fd_);
        sock_fd_ = -1;
        return false;
      }
    }
    connected_ = true;
    std::cout << "Connected to " << host << ":" << port << "\n";
    return true;
  }

  ssize_t receive(void* buffer, size_t max_len) {
    if (!connected_ || sock_fd_ < 0) return -1;
    ssize_t bytes = recv(sock_fd_, buffer, max_len, 0);
    if (bytes == 0) {
      connected_ = false;
      return 0;
    } else if (bytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
      if (errno == ECONNRESET || errno == EPIPE) {
        connected_ = false;
        return -1;
      }
      std::cerr << "Client recv error: " << strerror(errno) << "\n";
      return -1;
    }
    std::cout << "Client received bytes: " << bytes << "\n";
    return bytes;
  }

  bool send_subscription(const std::vector<uint16_t>& symbol_ids) {
    if (!connected_) return false;
    size_t msg_size = sizeof(uint8_t) + sizeof(uint16_t) + symbol_ids.size() * sizeof(uint16_t);
    std::vector<uint8_t> buffer(msg_size);
    buffer[0] = MSG_TYPE_SUBSCRIBE;
    *reinterpret_cast<uint16_t*>(&buffer[1]) = symbol_ids.size();
    std::memcpy(&buffer[3], symbol_ids.data(), symbol_ids.size() * sizeof(uint16_t));
    ssize_t sent = send(sock_fd_, buffer.data(), buffer.size(), 0);
    return sent == static_cast<ssize_t>(buffer.size());
  }

  bool is_connected() const { return connected_; }

  void disconnect() {
    if (sock_fd_ >= 0) {
      close(sock_fd_);
      sock_fd_ = -1;
    }
    connected_ = false;
  }

  bool set_tcp_nodelay(bool enable) {
    int flag = enable ? 1 : 0;
    return setsockopt(sock_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
  }

  bool set_recv_buffer_size(size_t bytes) {
    int size = static_cast<int>(bytes);
    return setsockopt(sock_fd_, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) == 0;
  }

  bool set_send_buffer_size(size_t bytes) {
    int size = static_cast<int>(bytes);
    return setsockopt(sock_fd_, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) == 0;
  }

  bool set_socket_priority(int priority) {
#ifdef SO_PRIORITY
    return setsockopt(sock_fd_, SOL_SOCKET, SO_PRIORITY, &priority, sizeof(priority)) == 0;
#else
    (void)priority;
    return true;
#endif
  }

  int get_fd() const { return sock_fd_; }

 private:
  int sock_fd_;
  bool connected_;
};

}  // namespace mdfh
