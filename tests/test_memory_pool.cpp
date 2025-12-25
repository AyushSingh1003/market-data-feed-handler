#include "memory_pool.h"
#include <iostream>

using namespace mdfh;

int main() {
  MemoryPool pool(4, 128);
  uint8_t* a = pool.allocate();
  uint8_t* b = pool.allocate();
  uint8_t* c = pool.allocate();
  uint8_t* d = pool.allocate();
  uint8_t* e = pool.allocate();
  if (!a || !b || !c || !d) {
    std::cout << "FAIL\n";
    return 1;
  }
  if (e != nullptr) {
    std::cout << "FAIL\n";
    return 1;
  }
  pool.release(b);
  uint8_t* f = pool.allocate();
  if (f == nullptr) {
    std::cout << "FAIL\n";
    return 1;
  }
  std::cout << "PASS\n";
  return 0;
}
