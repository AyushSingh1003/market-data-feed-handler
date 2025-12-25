#pragma once

#include "protocol.h"
#include <atomic>
#include <vector>
#include <memory>

namespace mdfh {

class SymbolCache {
 public:
  explicit SymbolCache(size_t num_symbols) {
    states_.resize(num_symbols);
    for (size_t i = 0; i < num_symbols; ++i) {
      states_[i] = std::make_unique<MarketState>();
    }
  }

  void update_bid(uint16_t symbol_id, double price, uint32_t quantity, uint64_t timestamp) {
    if (symbol_id >= states_.size()) return;
    auto& state = *states_[symbol_id];
    state.best_bid.store(price, std::memory_order_release);
    state.bid_quantity.store(quantity, std::memory_order_release);
    state.last_update_time.store(timestamp, std::memory_order_release);
    state.update_count.fetch_add(1, std::memory_order_relaxed);
  }

  void update_ask(uint16_t symbol_id, double price, uint32_t quantity, uint64_t timestamp) {
    if (symbol_id >= states_.size()) return;
    auto& state = *states_[symbol_id];
    state.best_ask.store(price, std::memory_order_release);
    state.ask_quantity.store(quantity, std::memory_order_release);
    state.last_update_time.store(timestamp, std::memory_order_release);
    state.update_count.fetch_add(1, std::memory_order_relaxed);
  }

  void update_trade(uint16_t symbol_id, double price, uint32_t quantity, uint64_t timestamp) {
    if (symbol_id >= states_.size()) return;
    auto& state = *states_[symbol_id];
    state.last_traded_price.store(price, std::memory_order_release);
    state.last_traded_quantity.store(quantity, std::memory_order_release);
    state.last_update_time.store(timestamp, std::memory_order_release);
    state.update_count.fetch_add(1, std::memory_order_relaxed);
  }

  void update_quote(uint16_t symbol_id,
                    double bid_price, uint32_t bid_qty,
                    double ask_price, uint32_t ask_qty,
                    uint64_t timestamp) {
    if (symbol_id >= states_.size()) return;
    auto& state = *states_[symbol_id];
    state.best_bid.store(bid_price, std::memory_order_release);
    state.bid_quantity.store(bid_qty, std::memory_order_release);
    state.best_ask.store(ask_price, std::memory_order_release);
    state.ask_quantity.store(ask_qty, std::memory_order_release);
    state.last_update_time.store(timestamp, std::memory_order_release);
    state.update_count.fetch_add(1, std::memory_order_relaxed);
  }

  struct Snapshot {
    double best_bid;
    double best_ask;
    uint32_t bid_quantity;
    uint32_t ask_quantity;
    double last_traded_price;
    uint32_t last_traded_quantity;
    uint64_t last_update_time;
    uint64_t update_count;
  };

  Snapshot get_snapshot(uint16_t symbol_id) const {
    if (symbol_id >= states_.size()) {
      return Snapshot{};
    }
    const auto& state = *states_[symbol_id];
    Snapshot snap;
    snap.best_bid = state.best_bid.load(std::memory_order_acquire);
    snap.best_ask = state.best_ask.load(std::memory_order_acquire);
    snap.bid_quantity = state.bid_quantity.load(std::memory_order_acquire);
    snap.ask_quantity = state.ask_quantity.load(std::memory_order_acquire);
    snap.last_traded_price = state.last_traded_price.load(std::memory_order_acquire);
    snap.last_traded_quantity = state.last_traded_quantity.load(std::memory_order_acquire);
    snap.last_update_time = state.last_update_time.load(std::memory_order_acquire);
    snap.update_count = state.update_count.load(std::memory_order_relaxed);
    return snap;
  }

  uint64_t get_update_count(uint16_t symbol_id) const {
    if (symbol_id >= states_.size()) return 0;
    return states_[symbol_id]->update_count.load(std::memory_order_relaxed);
  }

  size_t size() const { return states_.size(); }

 private:
  std::vector<std::unique_ptr<MarketState>> states_;
};

}  // namespace mdfh
