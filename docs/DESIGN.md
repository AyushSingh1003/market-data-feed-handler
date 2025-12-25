# Design Document – Market Data Feed Handler

## Overview
- Single TCP server (`exchange_simulator`) generates synthetic market data and broadcasts to connected clients.
- Single TCP client (`feed_handler`) connects, parses a binary protocol, updates a lock‑free symbol cache, and renders a terminal view.
- Design favors simplicity, portability (Linux/macOS), and low overhead data movement.

## Components
- `ExchangeSimulator`
  - Networking: `AF_INET` TCP, `TCP_NODELAY`, `SO_REUSEADDR/REUSEPORT`, `SO_NOSIGPIPE` on macOS.
  - I/O loop: `epoll` on Linux, `select` on macOS; server socket is non‑blocking.
  - Tick pipeline: `generate_ticks()` → build `TradeMessage`/`QuoteMessage` → assign global sequence → `broadcast_message()` to all clients.
  - Backpressure: basic; errors (`EPIPE/ECONNRESET`) drop client; improvement path listed below.
- `TickGenerator`
  - GBM‑style evolution; per‑symbol volatility/spread/volume.
  - Produces bid/ask and trade price and increments local counters for symbol analytics.
- `protocol.h`
  - Packed structs for header/payload + XOR checksum.
  - Observed sizes: Trade = 80 bytes, Quote = 92 bytes (from `sizeof(...)`).
  - Host endian; same codebase for producer/consumer avoids marshaling.
- `BinaryParser`
  - Streaming parser with internal buffer; demux by `msg_type` and `sizeof(...)`.
  - Checksum verification; global sequence gap tracking (see below).
- `SymbolCache`
  - Per‑symbol `MarketState` of atomics; update methods are write‑barriered, snapshots are read‑barriered.
- `LatencyTracker`
  - Records ns latency using producer timestamp and consumer `CLOCK_REALTIME`; percentiles from histogram.
- `MarketDataSocket`
  - Non‑blocking connect with `select` timeout; `recv` loop, simple subscription send.

## Sequence Numbers
- Requirement: strictly increasing sequence numbers per the assignment.
- Implementation: single global `std::atomic<uint32_t> global_sequence_`; incremented once per produced message.
- Client parser now tracks gaps globally (`last_sequence_`), not per symbol.
- Trade‑off: mixing independent symbol streams into one globally ordered stream can introduce reordering relative to per‑symbol timelines; strict global monotonicity is preserved.

## Subscriptions
- Client sends `MSG_TYPE_SUBSCRIBE {count, symbol_ids[]}` on connect.
- Server currently streams all symbols to all clients; subscription list is accepted for compatibility but not filtered yet.
- Trade‑off: simplicity and continuous load for throughput testing; future work adds per‑client symbol filters.

## Data Flow
- Server: `TickGenerator` → build message → global sequence → `send()` to clients.
- Client: `recv()` → `BinaryParser::parse()` → checksum → callbacks → `SymbolCache` & `LatencyTracker` → terminal view.

## Portability
- Linux: non‑blocking sockets + `epoll` for readiness.
- macOS: `SO_NOSIGPIPE`, blocking `send()` and `select()` on the server accept path (clearer semantics, fewer syscalls).
- Both: `TCP_NODELAY` to minimize latency.

## Integrity & Safety
- XOR checksum is O(n) and inexpensive; suitable for demo. Production should use CRC32/XXH32.
- Packed structs reduce overhead; avoid cross‑language framing complications by sharing headers.

## Performance & Scaling (Future Work)
- Replace blocking `send()` with edge‑triggered non‑blocking writes and per‑client outbound ring buffers.
- Batch serialization (coalesce multiple messages per `send`), `sendmmsg()` on Linux.
- Disable debug logging in hot path, pin threads, tune socket buffers.
- Consider per‑client subscription filtering to reduce broadcast volume.

## Known Limitations
- Host‑endian binary protocol (not wire‑portable across different endianness).
- No retransmission or sequence recovery; gaps are counters only.
- Subscription filtering not yet implemented.
- Debug output impacts throughput; disable in benchmarks.

