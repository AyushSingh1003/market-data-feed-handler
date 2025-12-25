#!/bin/bash
# scripts/run_client.sh

cd "$(dirname "$0")/.."
./build/feed_handler 127.0.0.1 9876
