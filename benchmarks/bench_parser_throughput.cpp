#include "binary_parser.h"
#include "protocol.h"
#include <vector>
#include <chrono>
#include <iostream>

using namespace mdfh;

int main() {
  size_t N = 200000;
  std::vector<uint8_t> buf;
  buf.reserve(N * sizeof(TradeMessage));
  for (size_t i = 0; i < N; ++i) {
    TradeMessage t{};
    t.header.msg_type = MSG_TYPE_TRADE;
    t.header.symbol_id = 1;
    t.header.sequence = static_cast<uint32_t>(i + 1);
    t.header.timestamp = static_cast<uint64_t>(i);
    t.payload.price = 1000.0 + i;
    t.payload.quantity = 10;
    t.compute_checksum();
    size_t off = buf.size();
    buf.resize(off + sizeof(t));
    std::memcpy(buf.data() + off, &t, sizeof(t));
  }
  BinaryParser parser;
  auto start = std::chrono::steady_clock::now();
  size_t pos = 0;
  while (pos < buf.size()) {
    size_t chunk = std::min<size_t>(8192, buf.size() - pos);
    parser.parse(buf.data() + pos, chunk);
    pos += chunk;
  }
  auto end = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(end - start).count();
  uint64_t processed = parser.get_messages_processed();
  double throughput = processed / seconds;
  std::cout << "parser_msgs=" << processed << " time_s=" << seconds << " throughput_msg_s=" << throughput << "\n";
  return 0;
}
