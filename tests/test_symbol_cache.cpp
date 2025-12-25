#include "symbol_cache.h"
#include <iostream>

using namespace mdfh;

int main() {
  SymbolCache cache(3);
  cache.update_quote(1, 100.0, 10, 101.0, 12, 1);
  cache.update_trade(1, 100.5, 8, 2);
  auto snap = cache.get_snapshot(1);
  if (snap.best_bid != 100.0 || snap.best_ask != 101.0 || snap.last_traded_price != 100.5 || snap.bid_quantity != 10 || snap.ask_quantity != 12) {
    std::cout << "FAIL\n";
    return 1;
  }
  if (cache.get_update_count(1) < 2) {
    std::cout << "FAIL\n";
    return 1;
  }
  std::cout << "PASS\n";
  return 0;
}
