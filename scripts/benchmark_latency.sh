#!/bin/bash
# scripts/benchmark_latency.sh

cd "$(dirname "$0")/.."

echo "=== Latency Benchmark ==="
echo ""

echo "Starting server (500K msgs/sec)..."
./build/exchange_simulator 9876 500 500000 &
SERVER_PID=$!
sleep 2

echo "Running client for 30 seconds..."
timeout 30 ./build/feed_handler 127.0.0.1 9876 || true

kill $SERVER_PID 2>/dev/null || true

echo ""
echo "Benchmark complete. Check output for latency statistics."
