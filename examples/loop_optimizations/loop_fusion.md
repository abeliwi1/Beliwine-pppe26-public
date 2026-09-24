# Loop Fusion

Computing mean and variance in two separate passes versus one fused pass,
using the computational formula to break the data dependency that forces the
second pass. Worth **2.0x** — which is the theoretical ceiling, because this
loop is limited by memory bandwidth and fusion halves the memory traffic.

## Source

[loop_fusion.cpp](loop_fusion.cpp)

## Machine

AMD Ryzen AI 9 HX 370 (Zen 5, "Strix Point"), Linux, g++ 13.3. L3 is 16 MB;
the array here is 381 MB, so it is DRAM-resident and every pass is a cold read.

## The problem with two loops

Variance needs the mean, and the mean needs a full pass. The naive
implementation is therefore forced into two sequential passes over the data:

```cpp
// Pass 1: mean
double sum = 0.0;
for (int i = 0; i < n; i++) sum += data[i];
double mean = sum / n;

// Pass 2: variance — cannot start until mean is known
double m2 = 0.0;
for (int i = 0; i < n; i++) {
    double d = data[i] - mean;
    m2 += d * d;
}
double variance = m2 / n;
```

With N = 50M ints the array is 381 MB, far past the 16 MB L3. Pass 2 is a full
cold reload from DRAM.

## Fused — computational formula

The identity **Var(X) = E[X²] − E[X]²** lets `sum` and `sum_sq` accumulate
together in one loop. Both statistics are derived afterwards, with no second
read:

```cpp
double sum = 0.0, sum_sq = 0.0;
for (int i = 0; i < n; i++) {
    double x = data[i];
    sum    += x;
    sum_sq += x * x;
}
double mean     = sum / n;
double variance = (sum_sq / n) - mean * mean;
```

## Build

```bash
g++ -O1 -o fusion_O1 loop_fusion.cpp && ./fusion_O1
```

## Results

N = 50M ints (381 MB), `g++ -O1`.

| Version | Passes | Time | vs unfused |
|---|---:|---:|---:|
| Unfused (two loops) | 2 | 59.2 ms | baseline |
| **Fused (computational formula)** | **1** | **29.3 ms** | **2.02x** |
| Fused (Welford's algorithm) | 1 | 191.4 ms | 0.31x |

## Analysis

### Why 2.0x and not less

The theoretical ceiling is exactly 2x: two reads become one. Reaching it
means the loop is *purely* bandwidth-bound — the extra `x * x` multiply per
element in the fused version is free, absorbed by the out-of-order engine while
it waits on memory.

That is the regime fusion is for. The transformation does not reduce
arithmetic; it reduces trips to DRAM. If the loop were compute-bound, fusing it
would buy nothing.

**A useful diagnostic falls out of this.** Warming the core's clock before
measuring changes the cache-bound examples in this directory by ~1.4x, and
changes these numbers by almost nothing. A loop whose time does not respond to
core frequency is running at the speed of the memory system, not the core.

### Why Welford's is 3x slower, not faster

Welford's online algorithm is the textbook numerically-stable single-pass
method:

```cpp
for (int i = 0; i < n; i++) {
    double delta = data[i] - mean;
    mean += delta / (i + 1);
    m2   += delta * (data[i] - mean);
}
```

It reads the array once — the same memory traffic as the fused version — and is
**6.5x slower than that version** and 3.2x slower than doing two passes. Two
reasons, both about the loop body rather than the memory:

1. **A division per element.** `delta / (i + 1)` is a floating-point divide on
   every iteration, roughly 15–20 cycles against 4–5 for a multiply. At 50M
   iterations that alone dominates.

2. **A loop-carried dependency.** Each iteration's `mean` update depends on the
   previous iteration's `mean`. The out-of-order engine cannot overlap
   iterations, so the divides serialise instead of pipelining.

The result is a loop that halves the memory traffic and still loses badly,
because it turned a memory-bound loop into a compute-bound one. **Fusion is
only a win if the fused body stays cheap enough to remain memory-bound.**

### Numerical accuracy

The computational formula accumulates large `x²` values and subtracts two large
numbers at the end, which risks catastrophic cancellation when the values are
large and the variance is small. Here the delta against the two-pass result is
~0.88 on a variance of ~3.58 × 10⁸ — about 2.5 × 10⁻⁹ relative error, fine for
demonstration.

For production on large or high-dynamic-range data, prefer the two-pass formula
(numerically exact) or Welford's (stable), and accept the cost. The 2x here is
bought partly with precision, and that is an honest part of the trade.

### When Welford's is the right choice

When bandwidth is not the bottleneck: data already in cache, small `n`, or
stability mattering more than throughput. It is also the only option for a
streaming computation where elements arrive one at a time and cannot be stored
for a second pass.

## Key takeaways

1. **Fusion trades memory passes for a wider loop body.** It wins when the loop
   is memory-bound and the wider body stays cheap.

2. **The ceiling is the pass count.** Two reads to one is 2x and no more; hitting
   2.02x means nothing was left on the table.

3. **A real data dependency can often be restructured rather than obeyed.**
   Variance genuinely needs the mean — but not the *same* formulation of the
   variance. Changing the algebra removed the dependency.

4. **Check that the fused body is still cheap.** Welford's fuses perfectly and
   loses 3.2x, because a divide and a serial dependency per element cost more
   than the DRAM pass it saved.

5. **If warming the clock doesn't change your timings, you are memory-bound.**
   Cheap to test and it tells you which optimisations can possibly help.
