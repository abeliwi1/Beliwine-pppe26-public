# Loop Fusion and the Reorder Window

The reorder buffer — what it does, why it is near-sighted rather than blocked,
and roughly how big the window is — is covered in
[README.md](README.md#the-reorder-buffer). This
 is the measurement: three versions of the same computation, what each
costs, and what the generated code shows.

## Source

[fuse_loops_rob.cpp](fuse_loops_rob.cpp)

## The three versions

All three compute `sum = Σ x[i]` and `sumsq = Σ x[i]²` over 64M floats.
`sumsq` never reads `sum`, so the two FADD chains are independent throughout —
what changes is how far apart the independent work sits in the instruction
stream.

**`two_passes`** — the chains are ~4 × 10⁸ instructions apart, far outside the
window:

```cpp
float s = 0;
for (int i = 0; i < n; i++) s += x[i];       // chain A

float ss = 0;
for (int i = 0; i < n; i++) ss += x[i]*x[i]; // chain B
```

**`fused_1acc`** — the same two chains, now one instruction apart:

```cpp
float s = 0, ss = 0;
for (int i = 0; i < n; i++) {
    s  += x[i];          // chain A
    ss += x[i] * x[i];   // chain B — adjacent, inside the window
}
```

**`fused_2acc`** — two accumulators per chain, so four independent chains cover
a 2-cycle latency:

```cpp
float s0=0, s1=0, q0=0, q1=0;
for (int i = 0; i < n; i += 2) {
    s0 += x[i];           s1 += x[i+1];
    q0 += x[i]*x[i];      q1 += x[i+1]*x[i+1];
}
```

Measured latencies on this core are FADD = 2 cycles and FMUL = **3**. The
multiply is the slower instruction and does not matter: `x[i]*x[i]` depends
only on the load, so it sits *off* the loop-carried path and feeds the add from
the side. Both chains are 2 cycles deep per element, which is what sets the
floors in the table below.

---

## Build

```bash
clang++ -O1 -o fuse_O1 fuse_loops_rob.cpp && ./fuse_O1
```

Use `-O1` for the cleanest reading.  `-O2`/`-O3` do **not** vectorise either
reduction — FP addition is not associative — but they do vectorise the
elementwise square and pack `fused_2acc`'s accumulators into lanes, which
speeds the baseline and the best case unequally.  See
[What `-O2` actually does](#what--o2-actually-does).

---

## Results (Apple M5, N=64M floats, 256 MB, Apple Clang 17)

<!-- FIXED: cycles/elem and CPI recomputed at the measured 4.44 GHz with
     FADD latency 2 (were computed at an assumed 4.0 GHz and latency 3).
     Measured times are unchanged -- only derived columns move. The old
     "2 loops x 3 cycles = 6" model appeared to match a measured 5.37 only
     because both constants were wrong in compensating directions. -->

At `-O1`:

| version | time | cycles/elem | CPI | floor | speedup |
|---------|------|-------------|-----|-------|---------|
| two_passes (serial loops) | 90.1 ms | 5.96 | 0.99 | 4.0 | 1.00x |
| fused_1acc (interleaved) | 52.6 ms | 3.48 | 0.58 | 2.0 | **1.71x** |
| fused_2acc (2 acc each) | 28.5 ms | 1.89 | 0.38 | 1.0 | **3.16x** |

At `-O2`, for comparison:

| version | time | cycles/elem | CPI | floor | speedup |
|---------|------|-------------|-----|-------|---------|
| two_passes | 64.5 ms | 4.27 | 0.71 | 4.0 | 1.00x |
| fused_1acc | 34.1 ms | 2.26 | 0.38 | 2.0 | 1.89x |
| fused_2acc | 28.5 ms | 1.89 | 0.38 | 1.0 | **2.26x** |

The `floor` column is `2 cycles × (passes / accumulators-per-chain)` — the cost
if the only thing you paid for was FADD latency. At `-O1` each version lands
1–2 cycles above its floor; that gap is loop overhead (increment, compare,
branch, load) that `-O1` does not remove. At `-O2` `two_passes` reaches 4.27
against a 4.0 floor, i.e. the overhead is essentially gone and what remains is
pure chain latency — which is the cleanest evidence that the model is right.

---

## Analysis

**two_passes → fused_1acc: 1.71x**

Fusing brings chain B inside the reorder window, where the hardware can
interleave it with chain A. Cycles/elem drops 5.96 → 3.48, against a floor that
halves from 4.0 to 2.0. The speedup is bounded by 2x and lands at 1.71x; the
shortfall is the per-iteration loop overhead, which fusion halves but does not
eliminate.

**fused_1acc → fused_2acc: 1.85x additional**

Two accumulators per chain gives four independent chains covering a 2-cycle
latency — comfortably more than needed. Cycles/elem falls to 1.89 against a 1.0
floor. Past this point the limit is no longer latency at all: it is instruction
throughput and loop overhead, which is why `-O2` reaches the identical 28.5 ms
and cannot do better.

**Ruling out the obvious alternative.** `two_passes` streams the 256 MB array
twice while the fused versions stream it once, so halved memory traffic is a
plausible competing explanation for the 1.71x. It isn't the cause here: at
~100 GB/s, streaming 256 MB costs about 2.6 ms, and even the doubled traffic of
`two_passes` implies a ~5 ms floor. Measured times are 90.1 and 52.6 ms — more
than an order of magnitude above the bandwidth floor. Both versions are
latency-bound, not bandwidth-bound, so the difference is the dependency
structure and the window. (Contrast [../pipeline/multiple_accs.md](../pipeline/multiple_accs.md), where
K=8 at 6.9 ms *does* approach its bandwidth floor and the scaling stops.)

---

## Key takeaways

1. **Latency costs you only on the critical path.**  FMUL is measured at 3
   cycles here and FADD at 2 — the multiply is the slower instruction, and it
   is irrelevant, because `x[i]*x[i]` depends only on the load and sits off the
   loop-carried chain. An instruction's cost is a property of the dependency
   graph, not of the instruction.

2. **Fusion and accumulators fix different things.**  Fusion changes *where*
   the independent work is; multiple accumulators change *how much* of it there
   is (see [../pipeline/multiple_accs.md](../pipeline/multiple_accs.md)). This example needs both: fusing
   gets 1.71x, and accumulators on top of that get 3.16x.
