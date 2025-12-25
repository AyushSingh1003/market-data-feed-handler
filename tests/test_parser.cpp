#include "binary_parser.h"
#include "protocol.h"
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace mdfh;

int main() {
  BinaryParser parser;
  uint64_t trade_count = 0;
  uint64_t quote_count = 0;
  parser.set_trade_callback([&](const TradeMessage&) { trade_count++; });
  parser.set_quote_callback([&](const QuoteMessage&) { quote_count++; });
  std::vector<uint8_t> buf;
  const size_t N = 10000;
  buf.reserve(N * sizeof(TradeMessage));
  for (size_t i = 0; i < N; ++i) {
    if (i % 2 == 0) {
      TradeMessage t{};
      t.header.msg_type = MSG_TYPE_TRADE;
      t.header.symbol_id = 1;
      t.header.sequence = static_cast<uint32_t>(i + 1);
      t.header.timestamp = static_cast<uint64_t>(i);
      t.payload.price = 1000.0 + i;
      t.payload.quantity = 10 + (i % 5);
      t.compute_checksum();
      size_t off = buf.size();
      buf.resize(off + sizeof(t));
      std::memcpy(buf.data() + off, &t, sizeof(t));
    } else {
      QuoteMessage q{};
      q.header.msg_type = MSG_TYPE_QUOTE;
      q.header.symbol_id = 2;
      q.header.sequence = static_cast<uint32_t>(i + 1);
      q.header.timestamp = static_cast<uint64_t>(i);
      q.payload.bid_price = 999.0 + i;
      q.payload.bid_qty = 10 + (i % 5);
      q.payload.ask_price = 1001.0 + i;
      q.payload.ask_qty = 12 + (i % 5);
      q.compute_checksum();
      size_t off = buf.size();
      buf.resize(off + sizeof(q));
      std::memcpy(buf.data() + off, &q, sizeof(q));
    }
  }
  size_t pos = 0;
  while (pos < buf.size()) {
    size_t chunk = std::min<size_t>(4096, buf.size() - pos);
    parser.parse(buf.data() + pos, chunk);
    pos += chunk;
  }
  uint64_t processed = parser.get_messages_processed();
  uint64_t gaps = parser.get_sequence_gaps();
  if (processed != N || gaps != 0 || trade_count != N / 2 || quote_count != N / 2) {
    std::cout << "FAIL\n";
    return 1;
  }
  std::cout << "PASS\n";
  return 0;
}
