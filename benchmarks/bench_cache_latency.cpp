#include "symbol_cache.h"
#include "latency_tracker.h"
#include <chrono>
#include <iostream>

using namespace mdfh;

int main() {
  SymbolCache cache(100);
  LatencyTracker tracker(100000);
  const size_t N = 100000;
  for (size_t i = 0; i < N; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    cache.update_quote(static_cast<uint16_t>(i % 100), 100.0 + i, 10, 101.0 + i, 12, i);
    auto t1 = std::chrono::steady_clock::now();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    tracker.record(ns);
  }
  auto stats = tracker.get_stats();
  std::cout << "samples=" << stats.sample_count
            << " p50_ns=" << stats.p50
            << " p99_ns=" << stats.p99
            << " p999_ns=" << stats.p999
            << " mean_ns=" << stats.mean
            << "\n";
  return 0;
}
