# Pipeline Stalls: Dependent Chains vs. Independent Accumulators

A CPU pipeline stalls when an instruction must wait on a value that hasn't
finished computing yet — a RAW (Read-After-Write) hazard. Two experiments
here show the same fix from two angles: a single accumulator forces every
step to wait on the last, while several independent accumulators give the
CPU other work to do while each result is still in flight.

## Source

[pipeline.cpp](pipeline.cpp)

## What is a RAW hazard

A pipelined CPU overlaps the stages (fetch, decode, execute, ...) of
consecutive instructions. That only works if the instructions are
independent. When instruction B reads a value that instruction A hasn't
finished writing, B can't enter the execute stage until A's result is
ready — the pipeline stalls, inserting bubble cycles instead of useful work.
A loop whose accumulator is read and written every iteration is a RAW
hazard repeated once per element.

## How it works

**Dependent chain — one accumulator, four ops each reading the line above:**

```cpp
long long x = 1;
for (int i = 0; i < ARRAY_SIZE; i++) {
    x = x + data[i];
    x = x ^ (x >> 1);
    x = x * 3;
    x = x ^ (x >> 2);
}
```

Every line depends on the one before it. No instruction here can start
before the previous one finishes.

**Independent chains — four accumulators, no cross-dependencies:**

```cpp
long long x1 = 1, x2 = 2, x3 = 3, x4 = 4;
for (int i = 0; i < ARRAY_SIZE; i += 4) {
    x1 = x1 + data[i];      x1 = x1 ^ (x1 >> 1);  x1 = x1 * 3;  x1 = x1 ^ (x1 >> 2);
    x2 = x2 + data[i + 1];  x2 = x2 ^ (x2 >> 1);  x2 = x2 * 3;  x2 = x2 ^ (x2 >> 2);
    x3 = x3 + data[i + 2];  x3 = x3 ^ (x3 >> 1);  x3 = x3 * 3;  x3 = x3 ^ (x3 >> 2);
    x4 = x4 + data[i + 3];  x4 = x4 ^ (x4 >> 1);  x4 = x4 * 3;  x4 = x4 ^ (x4 >> 2);
}
```

`x1`..`x4` share no dependency, so the CPU can overlap all four chains. A
second, simpler experiment in the same source (`singleAccumulator` vs.
`multipleAccumulators`, 1 vs. 8 accumulators over a plain `sum += data[i]`)
isolates the identical effect with no bit-mixing at all.

## Build

```bash
clang++ -std=c++17 -O1 -o pipeline pipeline.cpp && ./pipeline
./buildandrun.sh   # sweeps -O0 through -O3
```

## Results (Apple M-series, N=100M ints, min of 5 runs)

| Level | Dependent | Independent | Speedup | Single acc | Multi acc | Speedup |
|-------|-----------|-------------|---------|-------------|-----------|---------|
| -O0   | 496 ms    | 198 ms      | 2.51x   | 112 ms      | 76 ms     | 1.47x   |
| -O1   | 158 ms    | 44 ms       | 3.59x   | 23 ms       | 6 ms      | 3.83x   |
| -O2   | 0 ms*     | 45 ms       | —       | 0 ms*       | 2 ms      | —       |
| -O3   | 0 ms*     | 0 ms*       | —       | 0 ms*       | 0 ms*     | —       |

`*` — the effect doesn't vanish at higher `-O`; the *measurement* does, and
**not for the reason this file used to claim.** Dumping `-S` output shows
`dependentChain` and `independentChains` compile to **byte-identical
assembly at `-O1`, `-O2`, and `-O3`** — no vectorization (zero `v`/`q`
registers at any level, including where each collapses to `0 ms`), no
strength reduction, no constant-folding. Neither function's own generated
code changes at all past `-O0`. See Analysis, below, for what that actually
rules in and out.

## Verified optimization levels

Re-built and re-run at each level to confirm the table above (`clang++
-std=c++17`, same source, same machine):

| Level | Shows the effect? |
|---|---|
| `-O0` | **Yes** — 2.51x / 1.50x |
| `-O1` | **Yes** — 3.59x / 3.83x, the cleanest run |
| `-O2` | No — the dependent-chain side already reports 0 ms |
| `-O3` | No — both sides report 0 ms |

**Only `-O0` and `-O1` demonstrate this example as a timing comparison.**
From `-O2` on, the *measured* gap disappears — but not because the
compiler applied an equivalent fix to the code. See Analysis.

## Analysis

The 2.5–3.8x speedups from breaking the dependency chain are real and
visible at -O0/-O1, where the compiler still emits something close to a
literal instruction-per-line translation.

**This file used to say** that confirming what happened at `-O2`/`-O3`
would show either vectorization or a constant-folded result. That guess was
never actually checked against the assembly — and it's wrong. Dumping `-S`
for `dependentChain` and `independentChains` at every level shows:

```bash
clang++ -std=c++17 -O1 -S -o o1.s pipeline.cpp
clang++ -std=c++17 -O2 -S -o o2.s pipeline.cpp
clang++ -std=c++17 -O3 -S -o o3.s pipeline.cpp
diff <(awk '/^__Z14dependentChain/{f=1} f{print} /ret/{if(f)exit}' o1.s) \
     <(awk '/^__Z14dependentChain/{f=1} f{print} /ret/{if(f)exit}' o3.s)
# no differences
```

Both functions are byte-identical across `-O1`, `-O2`, and `-O3` — same
scalar `x`/`w`-register instructions, no vector register ever appears, and
the loop body itself is never removed or reduced. Since `dependentChain`
and `independentChains`'s own compiled code is unchanged, a genuinely
serial (or four-way scalar) 100-million-element loop cannot physically
finish in under a millisecond — the `0 ms` reading has to come from
somewhere other than the function itself running faster.

The most likely remaining explanation: both functions are fully inlined
into `main` from `-O1` on (no calls to either symbol by name at *any*
level — checkable the same way, grepping the mangled names), and `data`
never changes across the 5 `clobber()`-separated benchmark runs. Once
everything is inlined into one function, `-O3`'s more aggressive optimizer
plausibly recognizes the repeated pure computation on unchanging input and
eliminates the redundant re-execution across runs — a **benchmark-harness
effect**, not the compiler solving the RAW hazard. This is exactly the same
finding [pipeline.tempvar.md](https://github.com/randalburns/pppe26/blob/activities/examples/pipeline/pipeline.tempvar.md)
reaches independently for a different benchmark in this same file's family
— that example now lives on the `activities` branch, alongside
[Activity 1's solutions](https://github.com/randalburns/pppe26/blob/activities/activities/activity1.pipeline.solutions.md),
which walk through the full investigation, including where it runs out
before identifying the exact optimizer pass responsible.

This is the same phenomenon [../ILP/out_of_order.md](../ILP/out_of_order.md)
and [../ILP/sep_dependent.md](../ILP/sep_dependent.md) measure far more
precisely — reorder buffer behavior, cycles/elem, CPI, and a `K_min`
saturation formula, on doubles rather than ints. This file predates that
analysis and is the rougher, optimization-level-sweep version of the same
idea: it shows *that* the effect exists and *when* it's masked, without the
cycle-accounting those two go on to do.

## Key takeaways

1. **A loop-carried dependency chain serializes the pipeline** regardless of
   how many execution units are available, because each instruction can't
   start until the one before it finishes.
2. **Independent accumulators expose that concurrency** without changing
   the arithmetic — same total, more overlap.
3. **The effect survives past -O1; the measurement doesn't.** A 0 ms result
   at -O2/-O3 does *not* mean the compiler solved the RAW hazard for you —
   verified: the compiled functions are byte-identical, unvectorized, at
   every level from -O1 up. The vanishing measurement is a benchmark-harness
   effect (most likely redundant-call elimination once everything is
   inlined), not the compiler fixing the recurrence.
4. **A claim about "what the compiler did" isn't true until you've checked
   the assembly.** This file stated the vectorization/strength-reduction
   explanation for years without anyone dumping `-S` to confirm it — it
   sounded plausible and was wrong. Diffing the generated code across `-O`
   levels is one command; guessing about compiler internals is free and
   frequently incorrect.
