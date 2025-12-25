#include "exchange_simulator.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace mdfh;

std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

int main(int argc, char* argv[]) {
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  uint16_t port = 9876;
  size_t num_symbols = 100;
  uint32_t tick_rate = 100000;
  if (argc > 1) port = std::atoi(argv[1]);
  if (argc > 2) num_symbols = std::atoi(argv[2]);
  if (argc > 3) tick_rate = std::atoi(argv[3]);
  std::cout << "=== NSE Exchange Simulator ===\n";
  std::cout << "Port: " << port << "\n";
  std::cout << "Symbols: " << num_symbols << "\n";
  std::cout << "Target Rate: " << tick_rate << " msg/s\n\n";
  ExchangeSimulator simulator(port, num_symbols);
  simulator.set_tick_rate(tick_rate);
  if (!simulator.start()) {
    std::cerr << "Failed to start simulator\n";
    return 1;
  }
  std::cout << "Simulator running. Press Ctrl+C to stop.\n\n";
  std::thread sim_thread([&]() { simulator.run(); });
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  std::cout << "\nStopping simulator...\n";
  simulator.stop();
  sim_thread.join();
  return 0;
}
