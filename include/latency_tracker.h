#pragma once

#include <vector>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>

namespace mdfh {

class LatencyTracker {
 public:
  struct LatencyStats {
    uint64_t min;
    uint64_t max;
    uint64_t mean;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
    uint64_t p999;
    uint64_t sample_count;
  };

  explicit LatencyTracker(size_t max_samples = 1000000)
      : max_samples_(max_samples), write_index_(0) {
    samples_.resize(max_samples);
    histogram_buckets_ = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000,
                          100000, 200000, 500000, 1000000, 2000000, 5000000, 10000000};
    histogram_.resize(histogram_buckets_.size() + 1, 0);
  }

  void record(uint64_t latency_ns) {
    size_t idx = write_index_.fetch_add(1, std::memory_order_relaxed) % max_samples_;
    samples_[idx] = latency_ns;
    size_t bucket = get_bucket(latency_ns);
    histogram_[bucket] += 1;
    uint64_t current_min = min_.load(std::memory_order_relaxed);
    while (latency_ns < current_min) {
      if (min_.compare_exchange_weak(current_min, latency_ns, std::memory_order_relaxed)) {
        break;
      }
    }
    uint64_t current_max = max_.load(std::memory_order_relaxed);
    while (latency_ns > current_max) {
      if (max_.compare_exchange_weak(current_max, latency_ns, std::memory_order_relaxed)) {
        break;
      }
    }
    sum_.fetch_add(latency_ns, std::memory_order_relaxed);
  }

  LatencyStats get_stats() const {
    LatencyStats stats{};
    size_t count = std::min(write_index_.load(std::memory_order_acquire), max_samples_);
    if (count == 0) {
      return stats;
    }
    stats.sample_count = count;
    stats.min = min_.load(std::memory_order_relaxed);
    stats.max = max_.load(std::memory_order_relaxed);
    stats.mean = sum_.load(std::memory_order_relaxed) / count;
    std::vector<uint64_t> cumulative(histogram_.size());
    cumulative[0] = histogram_[0];
    for (size_t i = 1; i < histogram_.size(); ++i) {
      cumulative[i] = cumulative[i - 1] + histogram_[i];
    }
    uint64_t total = cumulative.back();
    if (total == 0) return stats;
    stats.p50 = find_percentile(cumulative, total, 0.50);
    stats.p95 = find_percentile(cumulative, total, 0.95);
    stats.p99 = find_percentile(cumulative, total, 0.99);
    stats.p999 = find_percentile(cumulative, total, 0.999);
    return stats;
  }

  void reset() {
    write_index_.store(0, std::memory_order_release);
    min_.store(UINT64_MAX, std::memory_order_relaxed);
    max_.store(0, std::memory_order_relaxed);
    sum_.store(0, std::memory_order_relaxed);
    for (auto& bucket : histogram_) {
      bucket = 0;
    }
  }

 private:
  size_t get_bucket(uint64_t latency_ns) const {
    for (size_t i = 0; i < histogram_buckets_.size(); ++i) {
      if (latency_ns < histogram_buckets_[i]) {
        return i;
      }
    }
    return histogram_buckets_.size();
  }

  uint64_t find_percentile(const std::vector<uint64_t>& cumulative, uint64_t total, double percentile) const {
    uint64_t target = static_cast<uint64_t>(total * percentile);
    for (size_t i = 0; i < cumulative.size(); ++i) {
      if (cumulative[i] >= target) {
        if (i == 0) return histogram_buckets_[0] / 2;
        if (i >= histogram_buckets_.size()) return histogram_buckets_.back();
        return histogram_buckets_[i];
      }
    }
    return histogram_buckets_.back();
  }

  size_t max_samples_;
  std::atomic<size_t> write_index_;
  std::vector<uint64_t> samples_;
  std::vector<uint64_t> histogram_buckets_;
  std::vector<uint64_t> histogram_;
  std::atomic<uint64_t> min_{UINT64_MAX};
  std::atomic<uint64_t> max_{0};
  std::atomic<uint64_t> sum_{0};
};

}  // namespace mdfh
