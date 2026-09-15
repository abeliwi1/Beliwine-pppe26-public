# Multiple Accumulators: Where the Speedup Stops

<!-- MOVED: was ILP/out_of_order.md.  It lives here because the transformation
     it measures is the pipeline lecture's, not an out-of-order one -- multiple
     accumulators work on a strictly in-order core too.  What it adds to
     pipeline.cpp is the sweep: how far the trick scales and what stops it. -->

> ### What this adds to [pipeline.cpp](pipeline.cpp)
>
> [`singleAccumulator` vs `multipleAccumulators`](pipeline.cpp) already showed
> that breaking a dependency chain is faster, and [pipeline.md](pipeline.md)
> explains why. This page assumes both and asks the questions that come after
> "is it faster?":
>
> * **How many accumulators do you actually need?** (Fewer than you'd guess —
>   two, on this core.)
> * **Why does adding more eventually stop helping?**
> * **What is the ceiling actually made of?**
>
> It answers them by sweeping K = 1, 2, 4, 8 over the same reduction, on
> `double` rather than `long long`, against a *measured* FADD latency and clock
> rather than assumed ones. The result is a speedup curve with three distinct
> regions — latency-bound, unit-bound, and bandwidth-bound — which is the
> groundwork for the roofline model later in the course.
>

## Source

[multiple_accs.cpp](multiple_accs.cpp)

## The chain, in one paragraph

`s += a[i]` makes every `fadd` read the register the previous `fadd` wrote, so
the loop retires one add per FADD latency — 2 cycles on this core, measured —
no matter how many FP units sit idle:

```cpp
double s = 0.0;
for (int i = 0; i < n; i++) s += a[i];      // one chain, 2 cycles per element
```

Splitting the accumulator into K independent copies gives K chains that
interleave, each still 2 cycles deep but overlapping:

```cpp
double s0=0, s1=0, s2=0, s3=0;              // four chains, interleaved
for (int i = 0; i < n; i += 4) {
    s0 += a[i];    s1 += a[i+1];
    s2 += a[i+2];  s3 += a[i+3];
}
```

The split is legal because addition is associative — the same justification as
in the pipeline lecture, and the reason the program's correctness check passes.
Everything from here on is about how far this goes.

---

## The pipeline saturation formula

The minimum accumulator count to saturate one FP unit at throughput 1/cycle:

<!-- FIXED: K_min recomputed at the measured 2-cycle latency (was 3).
     Also dropped the unsourced "Apple M5 has 4 [FP units]" claim -- Apple
     publishes none of this; what the data supports is a lower bound. -->
```
K_min = FADD_latency / issue_throughput = 2 / 1 = 2
```

At `K = 2 >= K_min`, a single FP unit is fully pipelined.  Adding more
accumulators beyond this saturates additional FP units — K=8 still beats K=4
here (6.9 vs 9.0 ms), so this core has at least three — until memory bandwidth
becomes the floor:

```
memory floor = array_size / memory_bandwidth = 512 MB / ~100 GB/s ≈ 5 ms
```

---

## Build

```bash
clang++ -O1 -o multiple_accs_O1 multiple_accs.cpp && ./multiple_accs_O1
```

<!-- FIXED: "every level from -O1 up" and "no optimization level ever" are
     both false under -ffast-math, which the AI Question below invites the
     reader to try. Scoped to default FP semantics so the experiment is a
     discovery rather than a contradiction of this page. -->
`-O1` is the clearest level, but it is **not** required — the effect is present
at `-O1`, `-O2` and `-O3` alike. See [What `-O2` actually does](#what--o2-actually-does)
below; the short version is that under default floating-point rules none of
those levels vectorises the reduction, so the serial dependency chain survives
all of them.

---

## Results (Apple M5, N=64M doubles, 512 MB, Apple Clang 17)

<!-- FIXED: cycles/elem and CPI recomputed at the measured 4.45 GHz (were
     computed at an assumed 4.0). Measured times are unchanged -- only the
     derived columns move, since both scale linearly with CPU_GHZ. The old
     figures were self-contradictory: serial is a strict dependency chain and
     cannot beat one FADD latency per element, yet 2.20 sat below the 3-cycle
     floor the page claimed. The "floor" column is new and shows the
     corrected numbers are consistent: each row lands just above latency/K. -->
At `-O1`:

| version | time | cycles/elem | CPI | latency/K floor | speedup |
|---------|------|-------------|-----|-----------------|---------|
| serial (1 accumulator) | 37.0 ms | 2.45 | 0.61 | 2.00 | 1.00x |
| 2 accumulators | 17.1 ms | 1.13 | 0.32 | 1.00 | 2.16x |
| 4 accumulators | 9.0 ms | 0.60 | 0.27 | 0.50 | **4.12x** |
| 8 accumulators | 6.9 ms | 0.46 | 0.23 | 0.25 | **5.39x** |

At `-O2`, for comparison:

| version | time | cycles/elem | CPI | latency/K floor | speedup |
|---------|------|-------------|-----|-----------------|---------|
| serial (1 accumulator) | 32.2 ms | 2.14 | 0.53 | 2.00 | 1.00x |
| 2 accumulators | 16.9 ms | 1.12 | 0.32 | 1.00 | 1.91x |
| 4 accumulators | 10.8 ms | 0.72 | 0.32 | 0.50 | 2.99x |
| 8 accumulators | 6.8 ms | 0.45 | 0.23 | 0.25 | **4.76x** |

---

## Analysis

**1 → 2 accumulators: 2.16x**
Two independent chains run concurrently.  The speedup is essentially 2x,
confirming the bottleneck is the serial dependency and not memory bandwidth —
if bandwidth were the limit, splitting the chain would have changed nothing.

<!-- FIXED: rewritten for 2-cycle latency. The old text credited K=4 with
     hiding the latency; at latency 2 that job is already done by K=2, which
     changes what the 2->4 gain is evidence *of*. -->
**2 → 4 accumulators: 1.90x additional**
Two chains already hide the 2-cycle FADD latency — `2acc` measures 1.13
cycles/elem against a 1.00 floor.  So this further gain is *not* latency
hiding; it is a second FP unit being engaged.  The speedup from 1 to 4 is
**4.12x**, far above the `FADD_latency = 2x` ceiling available from latency
hiding on a single unit, which is itself the proof that more than one unit is
in play.  Note cycles/elem (0.60) is well below 1: the machine is retiring
more than one element per cycle.

**4 → 8 accumulators: 1.30x additional**
Diminishing returns, as predicted — the pipeline is already full at K=4, so the
remaining gain comes from spreading across FP units rather than from hiding
latency.  Total 1 → 8 is **5.39x**.

<!-- FIXED: quote the bandwidth this benchmark actually achieves rather than
     only the ~100 GB/s spec figure. -->
**Why K=8 doesn't double K=4:**
The M5 sustains roughly 100 GB/s on sequential reads, so streaming 512 MB has a
floor near 5 ms.  At 6.9 ms K=8 is moving 512 MB at 72.5 GB/s — within ~40% of
that floor, and no amount of additional ILP can go below it.  This is the transition from a
*latency-bound* regime (where breaking chains helps) to a *bandwidth-bound*
one (where it cannot) — the same boundary the roofline model formalizes.

---

## What `-O2` actually does

<!-- FIXED: "No level ever" is falsified by -O2 -ffast-math; scoped to
     default FP rules so the AI Question below stays a real experiment. -->
**No level vectorises the reduction under default FP rules.** Floating-point addition is not
associative, so summing in a different order can give a different answer.
Vectorising `s += a[i]` means reassociating the sum, and the compiler refuses
without `-ffast-math`.  Counting true vector FP arithmetic (`fadd.2d`) in each
kernel:

| kernel | `-O1` | `-O2` | `-O3` |
|--------|:-----:|:-----:|:-----:|
| `sum_serial` | 0 | 0 | 0 |
| `sum_2acc` | 0 | 0 | 0 |
| `sum_4acc` | 0 | 0 | 0 |
| `sum_8acc` | 0 | **4** | **4** |

**But `-O2` is not doing nothing.** Here is `sum_serial`'s inner loop at `-O2`:

```
LBB0_5:
    ldp  q1, q2, [x10, #-32]   ← 128-bit loads: two doubles at a time
    mov  d3, v1[1]             ← extract the high lane back out
    ...
    fadd d0, d0, d1            ← and then eight *scalar* adds,
    fadd d0, d0, d3            ←   every one of them reading d0,
    fadd d0, d0, d2            ←   written by the one above it
    fadd d0, d0, d4
    fadd d0, d0, d5
    fadd d0, d0, d7
    fadd d0, d0, d6
    fadd d0, d0, d16
    subs x11, x11, #8
    b.ne LBB0_5
```

The loop is unrolled 8x and uses *vector loads* — but the arithmetic is eight
sequential scalar `fadd`s on `d0`.  The dependency chain is completely intact.
The compiler moved the data in parallel and was still forced to add it one at a
time.

**`sum_8acc` is the exception that proves the rule:**

```
LBB3_2:
    ldp     q5, q4, [x11, #32]
    ldp     q6, q7, [x11], #64
    fadd.2d v3, v3, v7      ← real SIMD: two accumulators per instruction
    fadd.2d v0, v0, v6
    fadd.2d v2, v2, v5
    fadd.2d v1, v1, v4
```

The eight accumulators the *source* declared get packed two-per-register into
four NEON registers.  This is legal precisely because it reassociates nothing:
`s0` and `s1` were already independent chains in the C++, so putting them in
two lanes of one register changes no summation order.

**The rule.** The compiler will happily vectorise independent work, and it will
pack independence you have already expressed into vector lanes.  It will not
manufacture independence that isn't in your source.  That is the whole thesis
of this example, stated from the compiler's side: `sum_8acc` gets SIMD *because*
you wrote eight accumulators, and `sum_serial` does not at `-O1`, `-O2` or
`-O3`, because you wrote one.
<!-- FIXED: was "never does, at any -O level" -- false with -ffast-math. -->

**AI Question**: _What happens if we set `-ffast-math`?_

**Practical consequence.** The ILP effect is visible at every level (5.39x at
`-O1`, 4.76x at `-O2`).  `-O1` is preferred here for clarity, not necessity:
at `-O2` the baseline itself speeds up (32.2 vs 37.0 ms, from unrolling and
wide loads) which compresses the ratio, and `sum_4acc` actually gets *slower*
(10.8 vs 9.0 ms) because Clang chooses an `ld4.2d` four-way de-interleaving
load that costs more than it saves.

---

## Key takeaways

1. **A loop-carried dependency chain serializes *any* CPU.**  In-order or
   out-of-order, wide or narrow: when every instruction reads the register the
   previous one writes, there is no schedule that overlaps them.  Out-of-order
   hardware does not help here — which is why the fix is a source change.
   <!-- FIXED: was "serializes an out-of-order CPU ... the ROB cannot reorder".
        True but misattributed: the chain defeats in-order machines equally,
        and nothing in this example needs a ROB. -->

2. **Independent accumulators expose ILP without changing the algorithm.**
   The transformation is purely a code restructuring — the mathematical result
   is identical.

3. **The saturation point is `K = latency × units`.**  For one FP unit at the
   measured 2-cycle latency, K=2 is sufficient.  On a machine with multiple
   units, more accumulators engage more units until memory bandwidth becomes
   the ceiling — here at K=8 and 72.5 GB/s.
   <!-- FIXED: was "3-cycle latency, K=3" -- latency measures 2 on this core. -->

4. **No optimization level eliminates this problem under default FP rules.**
   FP addition is not associative, so `-O1`/`-O2`/`-O3` will not reassociate
   `s += a[i]` into SIMD, and the serial chain survives `-O3` intact.  What the
   compiler *will* do is vectorise the accumulators you declared yourself:
   `sum_8acc` gets `fadd.2d` at `-O2` and `sum_serial` does not.  Under default
   semantics the compiler exploits independence; it does not create it.
   <!-- FIXED: was "at any level" / "never does" -- both false under
        -ffast-math, which is exactly the experiment posed above. -->


