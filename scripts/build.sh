#!/bin/bash
# build.sh - Build the entire project

set -e

echo "=== Building Market Data Feed Handler ==="

mkdir -p build

if command -v cmake >/dev/null 2>&1; then
  echo "Configuring with CMake..."
  cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native"
  echo "Compiling..."
  if command -v sysctl >/dev/null 2>&1; then
    NPROC=$(sysctl -n hw.ncpu)
  elif command -v nproc >/dev/null 2>&1; then
    NPROC=$(nproc)
  else
    NPROC=4
  fi
  cmake --build build -j"${NPROC}"
else
  echo "CMake not found, building directly with clang++"
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    src/server/exchange_simulator.cpp -o build/exchange_simulator
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    src/client/feed_handler.cpp -o build/feed_handler
  mkdir -p build/tests build/benchmarks
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    tests/test_parser.cpp -o build/tests/test_parser
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    tests/test_memory_pool.cpp -o build/tests/test_memory_pool
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    tests/test_symbol_cache.cpp -o build/tests/test_symbol_cache
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    benchmarks/bench_parser_throughput.cpp -o build/benchmarks/bench_parser_throughput
  clang++ -std=c++17 -O3 -Wall -Wextra -Iinclude -pthread \
    benchmarks/bench_cache_latency.cpp -o build/benchmarks/bench_cache_latency
fi

echo ""
echo "Build complete!"
echo "Binaries:"
echo " - build/exchange_simulator"
echo " - build/feed_handler"
echo " - build/tests/*"
echo " - build/benchmarks/*"
echo ""

echo "To run:"
echo " ./scripts/run_server.sh"
echo " ./scripts/run_client.sh"
