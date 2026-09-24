# Loop Unrolling

Manual unrolling of a data-driven loop over an array of unknown length, and
the **tail problem** — handling the leftover elements when `n` is not a
multiple of the unroll factor.

The headline result is that on this machine, at `-O1`, **unrolling does
nothing**: 1.01–1.04x, flat across every working set from 8 KB to 64 MB.
Duff's Device is consistently *slower*. What the example is really for is
explaining why, and what to do instead.

## Source

[loop_unrolling.cpp](loop_unrolling.cpp)

## Machine

AMD Ryzen AI 9 HX 370 (Zen 5, "Strix Point"), Linux, g++ 13.3. L1d 48 KB
per core, 12-way, 64-byte lines; L2 1 MB per core; L3 16 MB. Benchmarks are
pinned to a 5.13 GHz core and the clock is warmed before measuring — see
[the measurement notes](#measurement-notes) below, which matter more here
than the transformation does.

## Four versions

### 1. Scalar baseline
One add per iteration.

```cpp
long long s = 0;
for (int i = 0; i < n; i++)
    s += data[i];
```

### 2. 4x unroll with cleanup loop
Process `n - (n % 4)` elements four at a time, then a short scalar loop
finishes the 0–3 remainder. **One accumulator**, so this isolates the effect
of removing loop-control instructions and nothing else.

```cpp
int n4 = n - (n % 4);
for (; i < n4; i += 4) {
    s += data[i];
    s += data[i + 1];
    s += data[i + 2];
    s += data[i + 3];
}
for (; i < n; i++)   // cleanup tail
    s += data[i];
```

### 3. Duff's Device
A `switch` jumps into the *middle* of a `do/while` unrolled body so the first
iteration handles only the tail — no separate cleanup loop. Named after Tom
Duff (Bell Labs, 1983).

```cpp
int count = (n + 3) / 4;
switch (n % 4) {
    case 0: do { s += data[i++];  // falls through
    case 3:      s += data[i++];  // falls through
    case 2:      s += data[i++];  // falls through
    case 1:      s += data[i++];
            } while (--count > 0);
}
```

### 4. 4x unroll with four independent accumulators
Identical memory traffic and near-identical instruction count to version 2.
The only change is that the four adds no longer form a single dependency
chain.

```cpp
long long s0 = 0, s1 = 0, s2 = 0, s3 = 0;
for (; i < n4; i += 4) {
    s0 += data[i];
    s1 += data[i + 1];
    s2 += data[i + 2];
    s3 += data[i + 3];
}
for (; i < n; i++) s0 += data[i];
return s0 + s1 + s2 + s3;
```

## Build

```bash
g++ -O0 -o unroll_O0 loop_unrolling.cpp && ./unroll_O0
g++ -O1 -o unroll_O1 loop_unrolling.cpp && ./unroll_O1
g++ -O2 -o unroll_O2 loop_unrolling.cpp && ./unroll_O2
```

## Results

Array of `int`, odd length (`n % 4 == 1`, so every version exercises its tail
path). Repetition counts are chosen to hold the total element count roughly
constant, so times are comparable down a column.

### `-O1` — register allocation on, vectorisation off

| working set | scalar | 4x unroll | Duff's | 4x + 4 accums | cyc/elem |
|---|---:|---:|---:|---:|---:|
| 8 KB (L1)   | 82.2 ms | 79.4 ms (1.04x) | 86.2 ms (0.95x) | 49.9 ms (**1.65x**) | 1.03 |
| 32 KB (L1)  | 80.8 ms | 79.7 ms (1.01x) | 85.2 ms (0.95x) | 48.9 ms (**1.65x**) | 1.01 |
| 256 KB (L2) | 77.4 ms | 76.7 ms (1.01x) | 81.8 ms (0.95x) | 47.0 ms (**1.65x**) | 1.01 |
| 4 MB (L3)   | 83.4 ms | 82.1 ms (1.02x) | 87.9 ms (0.95x) | 50.7 ms (**1.65x**) | 1.02 |
| 64 MB (DRAM)| 85.4 ms | 84.4 ms (1.01x) | 89.9 ms (0.95x) | 52.2 ms (**1.64x**) | 1.04 |

### All three optimisation levels

Ratios are stable across working sets, so one row suffices:

| Flag | scalar cyc/elem | 4x unroll | Duff's Device | 4x + 4 accums |
|---|---:|---:|---:|---:|
| `-O0` | 2.42 | **1.35x** | **1.33x** | 1.59x |
| `-O1` | 1.01 | 1.01x | 0.95x | **1.65x** |
| `-O2` | 1.04 | **2.03x** | 0.94x | 2.03x |

## Analysis

### Why `-O1` shows nothing

The `cyc/elem` column is the whole explanation. The scalar loop runs at
**1.0 cycles per element at every level of the memory hierarchy** — including
64 MB, where every line comes from DRAM.

One element per cycle is exactly the latency of the dependent chain through
the accumulator: each `s += data[i]` must wait for the previous one, and an
integer add has 1-cycle latency. That is the floor, and the loop is sitting on
it everywhere. Meanwhile the loop-control instructions — increment, compare,
branch — issue on other ports in the same cycle, and the predictor gets the
back-edge right every time.

So loop control costs **zero cycles**, and unrolling removes instructions that
were already free. You cannot speed up a loop by deleting work that wasn't
costing anything.

The flatness across working sets is the other half of the point: a loop that
consumes one 4-byte element per cycle at 5.13 GHz asks for 20.5 GB/s, and
every level of this machine's memory system can supply that. The loop is
latency-bound on its own arithmetic, not on memory, at 8 KB and at 64 MB
alike.

### Why four accumulators *do* help

Version 4 attacks the dependency chain rather than the loop control. Four
independent accumulators are four independent chains, so four adds are in
flight at once and the loop is no longer paced by a single 1-cycle-latency
dependency. That is worth **1.65x**, and it is a different optimisation with a
different name — multiple accumulators, covered on its own in [../ILP/](../ILP/).

Unrolling is what *makes room* for it: you cannot write four accumulators
without an unrolled body. That is the honest case for the transformation.

### Why `-O0` "helps" for the wrong reason

At `-O0` nothing stays in a register between statements — the accumulator is
stored and reloaded through the stack frame on every single operation, and the
scalar loop costs 2.42 cycles/element instead of 1.0. Unrolling amortises the
loop-control *memory traffic* over four elements, which is why it shows 1.35x.

This is a measurement of the compiler being switched off, not of the
transformation. Duff's Device gains at `-O0` for the same reason and loses
everywhere else.

### Why `-O2` changes the answer

At `-O2` the unrolled versions jump to 2.03x — but not because unrolling
suddenly started paying. Checked with `-fopt-info-vec`, GCC vectorises exactly
two of the four kernels:

```
loop_unrolling.cpp:235:14: optimized: loop vectorized using 16 byte vectors
loop_unrolling.cpp:294:14: optimized: loop vectorized using 16 byte vectors
```

Those are `sum_unrolled4` and `sum_unrolled4_acc`. GCC does **not** vectorise
the scalar loop, and does not vectorise Duff's Device at all. The unrolled body
is what hands the vectoriser four independent loads to pack; the scalar loop
presents one element at a time and it declines.

Note also that versions 2 and 4 converge at `-O2` (2.03x each). Once the
vectoriser has the body, it does not care how you arranged the accumulators.

### Why Duff's Device loses

Consistently 0.94–0.96x from `-O1` up. The switch-based entry reaches the
middle of the loop body through an indirect jump, which predicts worse than the
ordinary back-edge of a counted loop. On a PDP-11 in 1983 it was a real win; on
an out-of-order superscalar core with a good branch predictor it costs a few
percent and buys nothing.

## Measurement notes

Two properties of this machine will silently corrupt every number above if
ignored, and both are handled in the source:

1. **Heterogeneous cores.** cpu0–3 are Zen 5 cores at 5.13 GHz; cpu4–11 are
   Zen 5c "dense" cores at 3.17 GHz. An unpinned run lands wherever the
   scheduler puts it — a 1.6x spread from the same binary. The benchmark pins
   itself to cpu0.

2. **The clock only boosts on high IPC.** `amd-pstate-epp` in its default
   powersave / balance_performance mode raises the clock in response to
   instructions-per-cycle, not to the core being busy. A memory-bound loop has
   low IPC and never triggers a boost, so it runs the whole way at ~3.59 GHz.
   Taking the minimum of five repetitions does **not** rescue you — every
   repetition is equally slow. The benchmark spins on a high-IPC loop first and
   prints the measured clock before and after the run, so a drifting clock is
   visible rather than silent.

A third trap is specific to this example: the array and its length are the same
on every repetition, so the call is loop-invariant and the compiler is entitled
to hoist it out and run it once. The benchmark perturbs one element per
repetition to prevent that. Without it the whole sweep reports `0.0 ms`, which
is at least a conspicuous way to fail.

## Key takeaway

**Loop unrolling is an enabler, not an optimisation.** On a modern
out-of-order core, loop-control overhead is already hidden — it issues in
parallel with real work and costs nothing, so removing it returns nothing.

What unrolling does is restructure the loop body so that *something else* can
happen: multiple independent accumulators that break a dependency chain
(1.65x by hand at `-O1`), or a body wide enough for the auto-vectoriser to
pack (2.03x at `-O2`). Unroll when you have a reason to want the wider body.
Unrolling for its own sake, and hand-written tail tricks like Duff's Device,
are for machines that no longer exist.
