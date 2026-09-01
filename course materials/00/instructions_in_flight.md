# How many instructions are in flight on this M5?

A working transcript: applying the "core_count x pipeline_depth x vector_width"
formula from [00.Intro.ipynb](../00.Intro.ipynb) to real numbers, then measuring
what actually limits it.

---

**Q: ok, let's answer the question, considering pipeline parallelism vector
parallelism how many active instructions need to be in flight at the same time
on this M5 machine?**

Using the numbers already measured elsewhere in this repo (not the generic
formula's placeholders):

- **Pipeline (superscalar issue width):** 10-wide OOO engine, sustaining ~7
  instructions/cycle in practice (measured in
  [speculative_execution.md](../../ILP/speculative_execution.md) via the
  CPI=0.14 sorted-branch result)
- **Vector width:** 4 lanes/instruction — 128-bit NEON, FP32 (confirmed by
  [peak_flops.cpp](../../roofline/roofline_example/peak_flops.cpp)'s 4-FMA-unit
  GFLOPS ceiling)

**In flight (pipeline x vector, no core multiplier):**
- theoretical: 10 x 4 = **40** scalar-op-equivalents/cycle
- sustained: 7 x 4 = **28** scalar-op-equivalents/cycle

One caveat worth keeping visible: this is instructions *issuing* per cycle,
not the same as true ROB occupancy (the actual count of not-yet-retired
in-flight instructions), which is a larger number Apple doesn't publish for
M5 and that nothing in this repo measures directly — so "40" is a defensible
lower-bound estimate for the lecture, not a verified microarchitectural spec.

---

**Q: that limits sustained w.r.t. theoretical**

**Q: can you write and execute a simple micro benchmark to measure?**

Confirmed this is actually running on the M5 (4 P-cores + 6 E-cores, matches
the repo's docs). Result, sweeping K independent integer-add accumulator
chains over an L1-resident array (~64 KB, re-read 500K times, so it's
ALU-bound, not memory-bound):

```
version              time(ms)      cyc/add      IPC   vs K=1
1 accumulator            928.6       1.0469     0.96    1.00x
2 accumulators           594.5       0.6703     1.49    1.56x
4 accumulators           353.3       0.3984     2.51    2.63x
6 accumulators           281.4       0.3173     3.15    3.30x
8 accumulators           246.9       0.2784     3.59    3.76x
12 accumulators          226.3       0.2551     3.92    4.10x
16 accumulators          216.0       0.2435     4.11    4.30x
```

What this shows about the 10-wide-vs-7-sustained gap:

- K=1 gives IPC ≈ 1 (expected — single-cycle `add`, one dependency chain, one
  issue/cycle)
- IPC climbs with independent streams, but **saturates around 4 adds/cycle**
  by K≈12–16 — nowhere near 10
- But each `add` here is paired with a `load` (`a[i]`) to feed it — so real
  front-end throughput at saturation is ≈ 2 x 4 ≈ **8 instructions/cycle**
  (loads + adds), which *is* close to the quoted 10-wide figure

So the gap isn't really "front-end can't keep up" — it's that **hitting
10-wide requires a mix of instruction types landing on different port
categories in the same cycle**. A homogeneous stream (all loads+adds)
saturates on how many of *that* port type exist (here, ~4 ALU results/cycle),
regardless of how independent the work is. The
`ILP/speculative_execution.md` example hits ~7 because its loop naturally
mixes loads, compares, and branches — more port diversity than a pure add
sweep, so it gets closer to the ceiling.

The benchmark: [ipc_sweep.cpp](ipc_sweep.cpp).
