#pragma once

#include <cstdint>
#include <cstring>
#include <atomic>
#include <time.h>

namespace mdfh {

constexpr uint16_t MSG_TYPE_TRADE = 0x01;
constexpr uint16_t MSG_TYPE_QUOTE = 0x02;
constexpr uint16_t MSG_TYPE_HEARTBEAT = 0x03;
constexpr uint16_t MSG_TYPE_SUBSCRIBE = 0xFF;

struct MessageHeader {
  uint16_t msg_type;
  uint16_t symbol_id;
  uint32_t sequence;
  uint64_t timestamp;

  void set_timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    timestamp = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
  }
} __attribute__((packed));

struct TradePayload {
  double price;
  uint32_t quantity;
} __attribute__((packed));

struct QuotePayload {
  double bid_price;
  uint32_t bid_qty;
  double ask_price;
  uint32_t ask_qty;
} __attribute__((packed));

struct TradeMessage {
  MessageHeader header;
  TradePayload payload;
  uint32_t checksum;

  void compute_checksum() {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
    uint32_t xor_sum = 0;
    for (size_t i = 0; i < sizeof(*this) - sizeof(checksum); ++i) {
      xor_sum ^= data[i];
    }
    checksum = xor_sum;
  }

  bool validate_checksum() const {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
    uint32_t xor_sum = 0;
    for (size_t i = 0; i < sizeof(*this) - sizeof(checksum); ++i) {
      xor_sum ^= data[i];
    }
    return xor_sum == checksum;
  }
} __attribute__((packed));

struct QuoteMessage {
  MessageHeader header;
  QuotePayload payload;
  uint32_t checksum;

  void compute_checksum() {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
    uint32_t xor_sum = 0;
    for (size_t i = 0; i < sizeof(*this) - sizeof(checksum); ++i) {
      xor_sum ^= data[i];
    }
    checksum = xor_sum;
  }

  bool validate_checksum() const {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
    uint32_t xor_sum = 0;
    for (size_t i = 0; i < sizeof(*this) - sizeof(checksum); ++i) {
      xor_sum ^= data[i];
    }
    return xor_sum == checksum;
  }
} __attribute__((packed));

struct HeartbeatMessage {
  MessageHeader header;
  uint32_t checksum;

  void compute_checksum() {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(this);
    uint32_t xor_sum = 0;
    for (size_t i = 0; i < sizeof(*this) - sizeof(checksum); ++i) {
      xor_sum ^= data[i];
    }
    checksum = xor_sum;
  }
} __attribute__((packed));

struct SubscriptionMessage {
  uint8_t cmd;
  uint16_t count;
  uint16_t symbol_ids[];
} __attribute__((packed));

struct alignas(64) MarketState {
  std::atomic<double> best_bid;
  std::atomic<double> best_ask;
  std::atomic<uint32_t> bid_quantity;
  std::atomic<uint32_t> ask_quantity;
  std::atomic<double> last_traded_price;
  std::atomic<uint32_t> last_traded_quantity;
  std::atomic<uint64_t> last_update_time;
  std::atomic<uint64_t> update_count;

  MarketState()
      : best_bid(0.0),
        best_ask(0.0),
        bid_quantity(0),
        ask_quantity(0),
        last_traded_price(0.0),
        last_traded_quantity(0),
        last_update_time(0),
        update_count(0) {}
};

}  // namespace mdfh
