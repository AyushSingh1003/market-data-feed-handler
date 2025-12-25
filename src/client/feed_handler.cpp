#include "protocol.h"
#include "market_data_socket.h"
#include "binary_parser.h"
#include "symbol_cache.h"
#include "latency_tracker.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <iomanip>
#include <vector>
#include <algorithm>

using namespace mdfh;

std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

void display_thread(const SymbolCache& cache, const LatencyTracker& latency,
                    const BinaryParser& parser, std::atomic<uint64_t>& msg_count) {
  auto start_time = std::chrono::steady_clock::now();
  uint64_t last_msg_count = 0;
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "\033[2J\033[H";
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    uint64_t current_count = msg_count.load(std::memory_order_relaxed);
    uint64_t delta = current_count - last_msg_count;
    uint64_t rate = delta * 2;
    last_msg_count = current_count;
    std::cout << "=== NSE Market Data Feed Handler ===\n";
    std::cout << "Connected to: localhost:9876\n";
    std::cout << "Uptime: " << std::setfill('0') << std::setw(2) << hours << ":"
              << std::setw(2) << minutes << ":" << std::setw(2) << seconds;
    std::cout << " | Messages: " << current_count << " | Rate: " << rate << " msg/s\n\n";
    std::vector<std::pair<uint16_t, uint64_t>> symbol_activity;
    for (size_t i = 0; i < cache.size(); ++i) {
      uint64_t count = cache.get_update_count(i);
      if (count > 0) {
        symbol_activity.push_back({static_cast<uint16_t>(i), count});
      }
    }
    std::sort(symbol_activity.begin(), symbol_activity.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::cout << std::left;
    std::cout << std::setw(8) << "Symbol"
              << std::setw(12) << "Bid"
              << std::setw(12) << "Ask"
              << std::setw(12) << "LTP"
              << std::setw(10) << "Updates\n";
    std::cout << std::string(70, '-') << "\n";
    size_t display_count = std::min<size_t>(20, symbol_activity.size());
    for (size_t i = 0; i < display_count; ++i) {
      uint16_t symbol_id = symbol_activity[i].first;
      auto snap = cache.get_snapshot(symbol_id);
      std::cout << std::setw(8) << ("SYM" + std::to_string(symbol_id))
                << std::setw(12) << std::fixed << std::setprecision(2) << snap.best_bid
                << std::setw(12) << snap.best_ask
                << std::setw(12) << snap.last_traded_price
                << std::setw(10) << snap.update_count << "\n";
    }
    auto stats = latency.get_stats();
    std::cout << "\nStatistics:\n";
    std::cout << " Parser Throughput: " << rate << " msg/s\n";
    std::cout << " End-to-End Latency: p50=" << stats.p50 / 1000 << "μs"
              << " p99=" << stats.p99 / 1000 << "μs"
              << " p999=" << stats.p999 / 1000 << "μs\n";
    std::cout << " Sequence Gaps: " << parser.get_sequence_gaps() << "\n";
    std::cout << " Cache Updates: " << current_count << "\n";
    std::cout << "\nPress Ctrl+C to quit\n";
    std::cout << std::flush;
  }
}

int main(int argc, char* argv[]) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  std::string host = "127.0.0.1";
  uint16_t port = 9876;
  if (argc > 1) host = argv[1];
  if (argc > 2) port = std::atoi(argv[2]);
  std::cout << "Starting Market Data Feed Handler...\n";
  SymbolCache cache(500);
  LatencyTracker latency;
  BinaryParser parser;
  std::atomic<uint64_t> msg_count{0};
  parser.set_trade_callback([&](const TradeMessage& msg) {
    uint64_t now_ns;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    now_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    uint64_t latency_ns = now_ns - msg.header.timestamp;
    latency.record(latency_ns);
    cache.update_trade(msg.header.symbol_id, msg.payload.price, msg.payload.quantity, msg.header.timestamp);
    msg_count++;
  });
  parser.set_quote_callback([&](const QuoteMessage& msg) {
    uint64_t now_ns;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    now_ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
    uint64_t latency_ns = now_ns - msg.header.timestamp;
    latency.record(latency_ns);
    cache.update_quote(msg.header.symbol_id, msg.payload.bid_price, msg.payload.bid_qty,
                       msg.payload.ask_price, msg.payload.ask_qty, msg.header.timestamp);
    msg_count++;
  });
  std::thread viz_thread(display_thread, std::ref(cache), std::ref(latency), std::ref(parser), std::ref(msg_count));
  MarketDataSocket socket;
  while (g_running) {
    if (!socket.is_connected()) {
      std::cout << "Connecting to " << host << ":" << port << "...\n";
      if (socket.connect(host, port, 5000)) {
        std::cout << "Connected successfully\n";
        std::vector<uint16_t> symbols;
        for (uint16_t i = 0; i < 500; ++i) {
          symbols.push_back(i);
        }
        socket.send_subscription(symbols);
      } else {
        std::cout << "Connection failed, retrying in 2 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        continue;
      }
    }
#ifdef __linux__
    static int epfd = -1;
    if (epfd == -1 && socket.is_connected()) {
      epfd = epoll_create1(0);
      struct epoll_event ev{};
      ev.events = EPOLLIN | EPOLLET;
      ev.data.fd = socket.get_fd();
      epoll_ctl(epfd, EPOLL_CTL_ADD, socket.get_fd(), &ev);
    }
    struct epoll_event events[8];
    int nfds = (epfd != -1) ? epoll_wait(epfd, events, 8, 100) : 0;
    if (nfds > 0) {
      for (int i = 0; i < nfds; ++i) {
        if (events[i].data.fd == socket.get_fd()) {
          uint8_t buffer[65536];
          ssize_t bytes = socket.receive(buffer, sizeof(buffer));
          if (bytes > 0) {
            parser.parse(buffer, static_cast<size_t>(bytes));
          } else if (bytes < 0) {
            std::cout << "Connection lost, reconnecting...\n";
            socket.disconnect();
            if (epfd != -1) { close(epfd); epfd = -1; }
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        }
      }
    }
#else
#ifdef __linux__
    static int epfd = -1;
    if (epfd == -1 && socket.is_connected()) {
      epfd = epoll_create1(0);
      struct epoll_event ev{};
      ev.events = EPOLLIN | EPOLLET;
      ev.data.fd = socket.get_fd();
      epoll_ctl(epfd, EPOLL_CTL_ADD, socket.get_fd(), &ev);
    }
    struct epoll_event events[8];
    int nfds = (epfd != -1) ? epoll_wait(epfd, events, 8, 100) : 0;
    if (nfds > 0) {
      for (int i = 0; i < nfds; ++i) {
        if (events[i].data.fd == socket.get_fd()) {
          uint8_t buffer[65536];
          ssize_t bytes = socket.receive(buffer, sizeof(buffer));
          if (bytes > 0) {
            parser.parse(buffer, static_cast<size_t>(bytes));
          } else if (bytes < 0) {
            std::cout << "Connection lost, reconnecting...\n";
            socket.disconnect();
            if (epfd != -1) { close(epfd); epfd = -1; }
            std::this_thread::sleep_for(std::chrono::seconds(1));
          }
        }
      }
    }
#else
    uint8_t buffer[65536];
    ssize_t bytes = socket.receive(buffer, sizeof(buffer));
    if (bytes > 0) {
      parser.parse(buffer, static_cast<size_t>(bytes));
    } else if (bytes == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    } else {
      std::cout << "Connection lost, reconnecting...\n";
      socket.disconnect();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif
#endif
  }
  viz_thread.join();
  std::cout << "\nShutting down...\n";
  return 0;
}
