#pragma once

#include <random>
#include <cmath>
#include <vector>

namespace mdfh {

class TickGenerator {
 public:
  struct SymbolConfig {
    double current_price;
    double drift;
    double volatility;
    double bid_ask_spread_pct;
    uint32_t avg_volume;
    uint32_t sequence;
  };

  TickGenerator(uint16_t symbol_id, double initial_price = 1000.0)
      : symbol_id_(symbol_id),
        rng_(std::random_device{}()),
        normal_dist_(0.0, 1.0),
        uniform_dist_(0.0, 1.0) {
    config_.current_price = initial_price;
    config_.drift = 0.0;
    config_.volatility = 0.02 + (uniform_dist_(rng_) * 0.04);
    config_.bid_ask_spread_pct = 0.0005 + (uniform_dist_(rng_) * 0.0015);
    config_.avg_volume = 1000 + static_cast<uint32_t>(uniform_dist_(rng_) * 9000);
    config_.sequence = 0;
  }

  void generate_tick(double dt = 0.001) {
    double z = box_muller();
    double drift_term = config_.drift * config_.current_price * dt;
    double diffusion_term = config_.volatility * config_.current_price * std::sqrt(dt) * z;
    config_.current_price += drift_term + diffusion_term;
    if (config_.current_price < 1.0) config_.current_price = 1.0;
    if (config_.current_price > 100000.0) config_.current_price = 100000.0;
    config_.sequence++;
  }

  std::pair<double, double> get_bid_ask() const {
    double spread = config_.current_price * config_.bid_ask_spread_pct;
    double mid = config_.current_price;
    return {mid - spread / 2.0, mid + spread / 2.0};
  }

  double get_trade_price() const {
    auto [bid, ask] = get_bid_ask();
    return bid + (ask - bid) * uniform_dist_(rng_);
  }

  uint32_t get_volume() const {
    double log_vol = std::log(config_.avg_volume) + normal_dist_(rng_) * 0.5;
    return static_cast<uint32_t>(std::exp(log_vol));
  }

  double get_current_price() const { return config_.current_price; }
  uint32_t get_sequence() const { return config_.sequence; }
  uint16_t get_symbol_id() const { return symbol_id_; }

  void set_drift(double drift) { config_.drift = drift; }
  void set_volatility(double vol) { config_.volatility = vol; }

 private:
  double box_muller() const {
    double u1 = uniform_dist_(rng_);
    double u2 = uniform_dist_(rng_);
    while (u1 <= 0.0) u1 = uniform_dist_(rng_);
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
  }

  uint16_t symbol_id_;
  SymbolConfig config_;
  mutable std::mt19937_64 rng_;
  mutable std::normal_distribution<double> normal_dist_;
  mutable std::uniform_real_distribution<double> uniform_dist_;
};

}  // namespace mdfh
