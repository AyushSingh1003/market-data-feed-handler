# Performance Analysis

## Environment
- OS: macOS (local loopback)
- Compiler: `clang++` with `-O3 -Wall -Wextra -Iinclude -pthread` (see `scripts/build.sh`)
- Server config (demo): `./build/exchange_simulator 9876 100 100000`
- Client config (demo): `./build/feed_handler 127.0.0.1 9876`

---

## Server Metrics (Observed from logs)
- Tick rate target: up to 100K msgs/sec
- Total ticks: periodic counters such as:
  - `Generated ticks: 100000`, `200000`, … `> 5,000,000`
- Broadcast sizes:
  - `Broadcasted 80 bytes …` (TradeMessage)
  - `Broadcasted 92 bytes …` (QuoteMessage)
- Disconnect handling:
  - `Send error fd=… errno=…` then client removal on `EPIPE/ECONNRESET`

---

## Client Metrics (Observed from logs)
- `Client received bytes: <N>` with common values `[80..736]`
  - Larger values indicate multiple messages coalesced per `recv()`
- Display thread shows:
  - `Parser Throughput: <msg/s>`
  - `End-to-End Latency: p50/p99/p999`
  - `Sequence Gaps: <count>`
  - `Cache Updates: <count>`

---

## Latency Measurement
- Producer writes ns timestamp into each header (`CLOCK_REALTIME`)
- Client subtracts current time and accumulates histogram
- Percentiles reported: `p50`, `p95`, `p99`, `p999`

---

## How To Benchmark
- Script: `./scripts/benchmark_latency.sh`
  - Starts server at `500000` msgs/sec for `500` symbols
  - Runs client for `30s`
  - Inspect client output lines for throughput and latency percentiles
- For peak numbers:
  - Run release build
  - Avoid debug logging in hot paths
  - Pin threads and tune socket buffers

---

## Notes
- Strict global sequence ordering implemented; gaps counter reflects jumps
- Subscription filtering is deferred; server streams all symbols to clients
- Host‑endian packed protocol; integrity via XOR checksum
