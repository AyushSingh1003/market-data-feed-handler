# Market Data Feed Handler

A high-performance, low-latency market data feed handler. Includes a TCP exchange simulator, binary protocol client, lock-free symbol cache, and real-time terminal visualization.

## Highlights

- Epoll-based server with non-blocking I/O and multi-client broadcast
- Client event loop with Linux epoll and macOS select() fallback
- Geometric Brownian Motion tick generation
- Fixed-block memory pool for fast buffer staging
- Binary parser with partial-read handling
- Lock-free symbol cache and latency tracker (p50/p99/p999)

## Repository Layout

```
src/
  server/
    exchange_simulator.cpp      # Server main
    tick_generator.cpp          # GBM implementation (stub; logic in include/)
    client_manager.cpp          # Client handling (stub; logic in include/)
  client/
    feed_handler.cpp            # Client main (network + visualize)
    socket.cpp                  # TCP client (stub; logic in include/)
    parser.cpp                  # Binary parser (stub; logic in include/)
    visualizer.cpp              # Terminal UI (stub)
  common/
    memory_pool.cpp             # Buffer pool (stub; logic in include/)
    cache.cpp                   # Symbol cache (stub; logic in include/)
    latency_tracker.cpp         # Latency utils (stub; logic in include/)
include/                        # Public headers (core implementations)
tests/                          # Unit tests
benchmarks/                     # Microbenchmarks
docs/                           # DESIGN.md, NETWORK.md, GBM.md, PERFORMANCE.md, QUESTIONS.md
scripts/                        # build/run/benchmark helpers
```

## Build

- Scripted build:
```bash
chmod +x scripts/*.sh
./scripts/build.sh
```

- CMake build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.ncpu 2>/dev/null || nproc || echo 4)
```

## Run

- Full demo:
```bash
./scripts/run_demo.sh
```

- Separate terminals:
```bash
# Terminal 1
./scripts/run_server.sh
# Terminal 2
./scripts/run_client.sh
```

- Binaries usage:
```bash
# Server: port num_symbols tick_rate_msg_per_sec
./build/exchange_simulator 9876 100 100000
# Client: host port
./build/feed_handler 127.0.0.1 9876
```

## Tests

- Build includes test binaries:
```bash
./build/tests/test_parser
./build/tests/test_memory_pool
./build/tests/test_symbol_cache
```
Each prints PASS/FAIL to stdout.

## Benchmarks

- Parser throughput:
```bash
./build/benchmarks/bench_parser_throughput
```
Outputs `parser_msgs`, `time_s`, `throughput_msg_s`.

- Cache update latency:
```bash
./build/benchmarks/bench_cache_latency
```
Outputs `samples`, `p50_ns`, `p99_ns`, `p999_ns`, `mean_ns`.

## Protocol

- Message types: trade `0x01`, quote `0x02`, heartbeat `0x03`, subscribe `0xFF`
- Each message begins with a fixed `MessageHeader { msg_type, symbol_id, sequence, timestamp }`
- Payloads:
  - Trade: `price`, `quantity`
  - Quote: `bid_price`, `bid_qty`, `ask_price`, `ask_qty`
- Checksum: simple XOR over bytes excluding checksum field

## Platform Notes

- Linux: server and client use epoll
- macOS: client uses select() fallback; server runs non-blocking and select-yield in network thread
- Requires a C++17 compiler (Clang/GCC) and POSIX sockets

## Documentation

- `docs/DESIGN.md` – System architecture, thread model, memory, concurrency, performance
- `docs/NETWORK.md` – Socket implementation details (server/client, buffer management)
- `docs/GBM.md` – Geometric Brownian Motion formulation and implementation
- `docs/PERFORMANCE.md` – Metrics, methodology, and analysis
- `docs/QUESTIONS.md` – Answers to critical thinking questions

## License

MIT
