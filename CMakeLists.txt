cmake_minimum_required(VERSION 3.15)
project(MarketDataFeedHandler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(exchange_simulator
  src/server/exchange_simulator.cpp
)
target_link_libraries(exchange_simulator pthread)

add_executable(feed_handler
  src/client/feed_handler.cpp
)
target_link_libraries(feed_handler pthread)

install(TARGETS exchange_simulator feed_handler
  RUNTIME DESTINATION bin
)
