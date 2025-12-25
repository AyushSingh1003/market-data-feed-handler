#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace mdfh {

class MemoryPool {
 public:
  MemoryPool(size_t block_count, size_t block_size)
      : block_count_(block_count),
        block_size_(block_size),
        storage_(block_count * block_size),
        free_list_() {
    free_list_.reserve(block_count_);
    for (size_t i = 0; i < block_count_; ++i) {
      free_list_.push_back(&storage_[i * block_size_]);
    }
  }

  uint8_t* allocate() {
    std::lock_guard<std::mutex> lock(mu_);
    if (free_list_.empty()) return nullptr;
    uint8_t* ptr = free_list_.back();
    free_list_.pop_back();
    return ptr;
  }

  void release(uint8_t* ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(mu_);
    free_list_.push_back(ptr);
  }

  size_t block_size() const { return block_size_; }
  size_t capacity() const { return block_count_; }

 private:
  size_t block_count_;
  size_t block_size_;
  std::vector<uint8_t> storage_;
  std::vector<uint8_t*> free_list_;
  std::mutex mu_;
};

}  // namespace mdfh

