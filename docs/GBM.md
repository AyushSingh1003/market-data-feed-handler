# Geometric Brownian Motion (GBM)

## 1. Mathematical Background
Stock prices are modeled using:
`dS = μS dt + σS dW`

This ensures:
- Positive prices
- Continuous evolution
- Realistic volatility dynamics

---

## 2. Discretization
Discrete evolution implemented via:
`S(t+dt) = S(t) + μS dt + σS √dt · N(0,1)`

Normal variates are generated using the Box–Muller transform in `TickGenerator`.

---

## 3. Parameter Selection
- `μ = 0` (neutral drift)
- `σ ∈ [0.02, 0.06]` randomized per symbol
- `dt = 0.001` seconds (1ms)
- Bid‑ask spread percentage ≈ `0.05%–0.2%` of the mid
- Trade volume sampled from a log‑normal distribution

---

## 4. Realism Considerations
- Prices are clamped to `[1.0, 100000.0]` to avoid non‑physical values
- Bid/ask derived from mid ± spread/2
- Trade price sampled uniformly across the spread
- Volume uses log‑space perturbations to emulate bursts and clustering
