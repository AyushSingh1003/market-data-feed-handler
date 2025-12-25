# Network Design

## 1. Server‑Side Design
- Multi‑client TCP server using `AF_INET`, `TCP_NODELAY`, `SO_REUSEADDR/REUSEPORT`
- Non‑blocking server socket
  - Linux: edge‑triggered `epoll` monitors accept readiness
  - macOS: `select()` monitors server socket readiness
- Broadcast occurs from the generator thread
  - `send()` uses `MSG_NOSIGNAL` on Linux and `SO_NOSIGPIPE` on macOS
  - On `EPIPE/ECONNRESET` the client is disconnected
- Improvement path
  - Switch to non‑blocking writes and per‑client outbound queues
  - Handle `EAGAIN` via deferred retries, coalesce batches, consider `sendmmsg` on Linux

---

## 2. Client‑Side Design
- Non‑blocking TCP socket with `TCP_NODELAY` and large `SO_RCVBUF`
- `recv()` loop:
  - Appends bytes to an internal parser buffer
  - Returns `0` on `EAGAIN/EWOULDBLOCK`; loop sleeps briefly to avoid spinning
- Subscription message sent after connect (`MSG_TYPE_SUBSCRIBE`); server currently streams all symbols

---

## 3. TCP Stream Handling
- TCP treated as a byte stream; messages can be fragmented or coalesced
- Parser frames messages using:
  - `MessageHeader` then `sizeof(TradeMessage)` or `sizeof(QuoteMessage)` based on `msg_type`
- Messages are processed only when fully available in the buffer

---

## 4. Connection Management
- Server removes clients on `EPIPE/ECONNRESET`
- Client detects disconnect when `recv()` returns `0` or fatal error and attempts reconnect
- Graceful shutdown via signals supported on both components

---

## 5. Error Handling
- Invalid `msg_type` is skipped by advancing one header
- Checksum validation guards against corrupted payloads
- Sequence gaps are counted; processing continues with latest data
