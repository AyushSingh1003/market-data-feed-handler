#pragma once

#include "protocol.h"
#include "tick_generator.h"
#include "memory_pool.h"
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <errno.h>
#include <random>

namespace mdfh {

class ExchangeSimulator {
 public:
  ExchangeSimulator(uint16_t port, size_t num_symbols = 100)
      : port_(port),
        num_symbols_(num_symbols),
        tick_rate_(100000),
        running_(false),
        server_fd_(-1),
        epoll_fd_(-1) {
    generators_.reserve(num_symbols);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> price_dist(100.0, 5000.0);
    for (size_t i = 0; i < num_symbols; ++i) {
      generators_.emplace_back(std::make_unique<TickGenerator>(i, price_dist(gen)));
    }
  }

  ~ExchangeSimulator() {
    stop();
    if (server_fd_ >= 0) close(server_fd_);
    if (epoll_fd_ >= 0) close(epoll_fd_);
  }

  bool start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
      std::cerr << "Failed to create socket\n";
      return false;
    }
    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      std::cerr << "Failed to bind to port " << port_ << "\n";
      return false;
    }
    if (listen(server_fd_, SOMAXCONN) < 0) {
      std::cerr << "Failed to listen\n";
      return false;
    }
#ifdef __linux__
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
      std::cerr << "Failed to create epoll\n";
      return false;
    }
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server_fd_;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);
#endif
    running_ = true;
    std::cout << "Exchange Simulator started on port " << port_ << "\n";
    std::cout << "Managing " << num_symbols_ << " symbols\n";
    return true;
  }

  void run() {
    std::thread generator_thread([this]() { generate_ticks(); });
    std::thread network_thread([this]() { handle_network(); });
    generator_thread.join();
    network_thread.join();
  }

  void stop() { running_ = false; }

  void set_tick_rate(uint32_t ticks_per_second) { tick_rate_ = ticks_per_second; }

 private:
  void handle_network() {
#ifdef __linux__
    const int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];
    while (running_) {
      int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100);
      for (int i = 0; i < nfds; ++i) {
        if (events[i].data.fd == server_fd_) {
          handle_new_connection();
        } else {
          if (events[i].events & (EPOLLHUP | EPOLLERR)) {
            handle_client_disconnect(events[i].data.fd);
          }
        }
      }
    }
#else
    while (running_) {
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(server_fd_, &read_fds);
      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
      int ret = select(server_fd_ + 1, &read_fds, nullptr, nullptr, &tv);
      if (ret > 0 && FD_ISSET(server_fd_, &read_fds)) {
        handle_new_connection();
      }
      static uint64_t t = 0;
      t++;
      if (t % 10 == 0) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::cout << "Network thread active, clients=" << clients_.size() << "\n";
      }
    }
#endif
  }

  void handle_new_connection() {
    while (true) {
      struct sockaddr_in client_addr{};
      socklen_t addr_len = sizeof(client_addr);
      int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &addr_len);
      if (client_fd < 0) {
        std::cerr << "accept() returned " << client_fd << " errno=" << errno << " (" << strerror(errno) << ")\n";
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
          std::cerr << "Accept failed\n";
        }
        break;
      }
      int flag = 1;
      setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
#ifdef SO_NOSIGPIPE
      int nosig = 1;
      setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &nosig, sizeof(nosig));
#endif
      // Make non-blocking
      int flags = fcntl(client_fd, F_GETFL, 0);
      fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
#ifdef __linux__
      struct epoll_event ev{};
      ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
      ev.data.fd = client_fd;
      epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);
#endif
      std::lock_guard<std::mutex> lock(clients_mutex_);
      clients_.push_back(client_fd);
      std::cout << "Client connected: fd=" << client_fd << " (total: " << clients_.size() << ")\n";
    }
  }

  void handle_client_disconnect(int client_fd) {
#ifdef __linux__
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
#endif
    close(client_fd);
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.erase(std::remove(clients_.begin(), clients_.end(), client_fd), clients_.end());
    std::cout << "Client disconnected: fd=" << client_fd << " (remaining: " << clients_.size() << ")\n";
  }

  void generate_ticks() {
    const uint64_t interval_ns = 1000000000ULL / tick_rate_;
    struct timespec req{}, rem{};
    req.tv_sec = 0;
    req.tv_nsec = interval_ns;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> symbol_dist(0, num_symbols_ - 1);
    std::uniform_real_distribution<> type_dist(0.0, 1.0);
    while (running_) {
      int symbol_idx = symbol_dist(gen);
      auto& generator = generators_[symbol_idx];
      generator->generate_tick();
      if (type_dist(gen) < 0.7) {
        QuoteMessage msg{};
        msg.header.msg_type = MSG_TYPE_QUOTE;
        msg.header.symbol_id = generator->get_symbol_id();
        msg.header.sequence = global_sequence_.fetch_add(1, std::memory_order_relaxed);
        msg.header.set_timestamp();
        auto [bid, ask] = generator->get_bid_ask();
        msg.payload.bid_price = bid;
        msg.payload.ask_price = ask;
        msg.payload.bid_qty = generator->get_volume();
        msg.payload.ask_qty = generator->get_volume();
        msg.compute_checksum();
        uint8_t* block = msg_pool_.allocate();
        if (block) {
          std::memcpy(block, &msg, sizeof(msg));
          broadcast_message(block, sizeof(msg));
          msg_pool_.release(block);
        } else {
          broadcast_message(&msg, sizeof(msg));
        }
      } else {
        TradeMessage msg{};
        msg.header.msg_type = MSG_TYPE_TRADE;
        msg.header.symbol_id = generator->get_symbol_id();
        msg.header.sequence = global_sequence_.fetch_add(1, std::memory_order_relaxed);
        msg.header.set_timestamp();
        msg.payload.price = generator->get_trade_price();
        msg.payload.quantity = generator->get_volume();
        msg.compute_checksum();
        uint8_t* block = msg_pool_.allocate();
        if (block) {
          std::memcpy(block, &msg, sizeof(msg));
          broadcast_message(block, sizeof(msg));
          msg_pool_.release(block);
        } else {
          broadcast_message(&msg, sizeof(msg));
        }
      }
      uint64_t c = tick_debug_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
      if (c % 100000 == 0) {
        std::cout << "Generated ticks: " << c << "\n";
        HeartbeatMessage hb{};
        hb.header.msg_type = MSG_TYPE_HEARTBEAT;
        hb.header.symbol_id = 0;
        hb.header.sequence = global_sequence_.fetch_add(1, std::memory_order_relaxed);
        hb.header.set_timestamp();
        hb.compute_checksum();
        broadcast_message(&hb, sizeof(hb));
      }
      nanosleep(&req, &rem);
    }
  }

  void broadcast_message(const void* data, size_t len) {
    std::vector<int> targets;
    {
      std::lock_guard<std::mutex> lock(clients_mutex_);
      targets = clients_;
    }
    std::vector<int> disconnected;
    for (int client_fd : targets) {
#ifdef MSG_NOSIGNAL
      ssize_t sent = send(client_fd, data, len, MSG_NOSIGNAL);
#else
      ssize_t sent = send(client_fd, data, len, 0);
#endif
      if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }
        if (errno == EPIPE || errno == ECONNRESET) {
          disconnected.push_back(client_fd);
        }
        std::cerr << "Send error fd=" << client_fd << " errno=" << errno << "\n";
      }
    }
    uint64_t s = send_debug_counter_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (s % 100000 == 0) {
      std::cout << "Broadcasted " << len << " bytes to " << targets.size() << " clients (events: " << s << ")\n";
    }
    for (int fd : disconnected) {
      handle_client_disconnect(fd);
    }
  }

  uint16_t port_;
  size_t num_symbols_;
  uint32_t tick_rate_;
  std::atomic<bool> running_;
  int server_fd_;
  int epoll_fd_;
  std::vector<std::unique_ptr<TickGenerator>> generators_;
  std::vector<int> clients_;
  std::mutex clients_mutex_;
  std::atomic<uint64_t> tick_debug_counter_{0};
  std::atomic<uint64_t> send_debug_counter_{0};
  std::atomic<uint32_t> global_sequence_{1};
  MemoryPool msg_pool_{4096, 128};
};

}  // namespace mdfh
