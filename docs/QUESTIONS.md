# Critical Thinking Questions

## How do you broadcast without blocking?
Current implementation broadcasts synchronously from the generator thread using `send()`. On Linux we use `MSG_NOSIGNAL`; on macOS we set `SO_NOSIGPIPE` and use blocking `send()`. If a client disconnects (`EPIPE`/`ECONNRESET`), it is removed. Improvement: switch to non‑blocking writes with per‑client queues and handle `EAGAIN` by retrying later.

## What if TCP send buffer fills?
With blocking `send()` (macOS path), the call may block. With non‑blocking sockets (Linux path), `send()` can return `EAGAIN`. We currently treat fatal errors and disconnect; we do not yet queue partial sends or back off on `EAGAIN`. Improvement: implement non‑blocking write buffers and batched sends.

## Why epoll edge‑triggered?
Linux uses edge‑triggered `epoll` for accept readiness to minimize syscalls. macOS uses `select()` on the server socket for readiness due to platform differences.

## How are partial TCP reads handled?
Client uses a streaming parser with an internal buffer. Bytes from `recv()` are appended; messages are processed only when a complete `MessageHeader` and the required payload size are available (`sizeof(TradeMessage)` or `sizeof(QuoteMessage)`).

## What happens on sequence gaps?
Server assigns a single global, strictly increasing sequence via `global_sequence_.fetch_add(1)`. Client tracks gaps globally and increments a counter when a jump is detected. Processing continues on gaps; no retransmission is implemented.

## Why no retransmission?
Market data processing values freshness over completeness; the latest state dominates decisions. The demo focuses on throughput/latency; recovery protocols are out of scope but could be layered separately.

## Why not per‑symbol sequences?
Per‑symbol sequencing is common and preserves per‑stream order. The assignment requires globally increasing sequence numbers, which we now implement. If per‑symbol streams are used, the client should track gaps per symbol.

## How is latency measured?
Producer sets a nanosecond timestamp in each header using `CLOCK_REALTIME`. Client records `now_ns − header.timestamp` and updates a histogram to compute `p50`, `p95`, `p99`, and `p999`.

## What integrity checks are used?
Packed C++ structs (host‑endian) with an XOR checksum over the message bytes validate integrity at low cost. Production feeds should upgrade to CRC32/XXH32 for stronger detection.

## What are the main trade‑offs?
Simplicity and performance over feature completeness: global sequence ordering meets the spec, subscription filtering is deferred, and the checksum is lightweight. Future work targets backpressure handling and batched I/O.
