#!/bin/bash
# scripts/run_demo.sh - Run complete demo

set -e

cd "$(dirname "$0")/.."

echo "=== Market Data Feed Handler Demo ==="
echo ""
echo "Starting Exchange Simulator..."

./build/exchange_simulator 9876 100 100000 &
SERVER_PID=$!

sleep 2

echo ""
echo "Starting Feed Handler Client..."
echo "Press Ctrl+C to stop"
echo ""

./build/feed_handler 127.0.0.1 9876

kill $SERVER_PID 2>/dev/null || true
