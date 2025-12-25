#pragma once

#include "protocol.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

namespace mdfh {

class BinaryParser {
 public:
  using TradeCallback = std::function<void(const TradeMessage&)>;
  using QuoteCallback = std::function<void(const QuoteMessage&)>;
  using HeartbeatCallback = std::function<void(const HeartbeatMessage&)>;

  BinaryParser() : buffer_size_(0), last_sequence_(0), sequence_gaps_(0), messages_processed_(0) {
    buffer_.resize(1024 * 1024);
  }

  void set_trade_callback(TradeCallback cb) { trade_callback_ = cb; }
  void set_quote_callback(QuoteCallback cb) { quote_callback_ = cb; }
  void set_heartbeat_callback(HeartbeatCallback cb) { heartbeat_callback_ = cb; }

  void parse(const void* data, size_t len) {
    if (buffer_size_ + len > buffer_.size()) {
      size_t new_cap = buffer_size_ + len + 65536;
      buffer_.resize(new_cap);
    }
    std::memcpy(buffer_.data() + buffer_size_, data, len);
    buffer_size_ += len;
    while (buffer_size_ >= sizeof(MessageHeader)) {
      const MessageHeader* header = reinterpret_cast<const MessageHeader*>(buffer_.data());
      size_t msg_size = get_message_size(header->msg_type);
      if (msg_size == 0) {
        std::cerr << "Unknown message type: 0x" << std::hex << header->msg_type << std::dec << "\n";
        shift_buffer(sizeof(MessageHeader));
        continue;
      }
      if (buffer_size_ < msg_size) {
        break;
      }
      process_message(buffer_.data(), msg_size);
      shift_buffer(msg_size);
    }
  }

  uint64_t get_sequence_gaps() const { return sequence_gaps_; }
  uint64_t get_messages_processed() const { return messages_processed_; }

 private:
  size_t get_message_size(uint16_t msg_type) const {
    switch (msg_type) {
      case MSG_TYPE_TRADE:
        return sizeof(TradeMessage);
      case MSG_TYPE_QUOTE:
        return sizeof(QuoteMessage);
      case MSG_TYPE_HEARTBEAT:
        return sizeof(HeartbeatMessage);
      default:
        return 0;
    }
  }

  void process_message(const void* data, size_t /*len*/) {
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);
    if (last_sequence_ > 0 && header->sequence != last_sequence_ + 1) {
      uint32_t gap = header->sequence - last_sequence_ - 1;
      sequence_gaps_ += gap;
    }
    last_sequence_ = header->sequence;
    switch (header->msg_type) {
      case MSG_TYPE_TRADE: {
        const TradeMessage* msg = reinterpret_cast<const TradeMessage*>(data);
        if (msg->validate_checksum()) {
          if (trade_callback_) trade_callback_(*msg);
          messages_processed_++;
        } else {
          std::cerr << "Trade message checksum failed\n";
        }
        break;
      }
      case MSG_TYPE_QUOTE: {
        const QuoteMessage* msg = reinterpret_cast<const QuoteMessage*>(data);
        if (msg->validate_checksum()) {
          if (quote_callback_) quote_callback_(*msg);
          messages_processed_++;
        } else {
          std::cerr << "Quote message checksum failed\n";
        }
        break;
      }
      case MSG_TYPE_HEARTBEAT: {
        const HeartbeatMessage* msg = reinterpret_cast<const HeartbeatMessage*>(data);
        if (msg->checksum) {
          if (heartbeat_callback_) heartbeat_callback_(*msg);
          messages_processed_++;
        }
        break;
      }
    }
  }

  void shift_buffer(size_t bytes) {
    if (bytes >= buffer_size_) {
      buffer_size_ = 0;
    } else {
      std::memmove(buffer_.data(), buffer_.data() + bytes, buffer_size_ - bytes);
      buffer_size_ -= bytes;
    }
  }

  std::vector<uint8_t> buffer_;
  size_t buffer_size_;
  uint32_t last_sequence_;
  uint64_t sequence_gaps_;
  uint64_t messages_processed_;
  TradeCallback trade_callback_;
  QuoteCallback quote_callback_;
  HeartbeatCallback heartbeat_callback_;
};

}  // namespace mdfh
