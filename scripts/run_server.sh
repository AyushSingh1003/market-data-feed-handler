#!/bin/bash
# scripts/run_server.sh

cd "$(dirname "$0")/.."
./build/exchange_simulator 9876 100 100000
