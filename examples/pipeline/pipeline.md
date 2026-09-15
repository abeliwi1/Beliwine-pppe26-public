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

_AI question: what are the pipeline steps on my processor and which stage does it read the value and stall?_

## How it works

**Dependent chain — one accumulator, four ops each reading the line above:**

```cpp
unsigned long long x = 1;
for (int i = 0; i < ARRAY_SIZE; i++) {
    unsigned long long v = (unsigned long long)data[i] | 1ull;
    x = x * v;
    x = x * (v + 2);
    x = x * (v + 4);
    x = x * (v + 6);
}
```

Every line depends on the one before it. No instruction here can start
before the previous one finishes.

**Independent chains — four accumulators, no cross-dependencies:**

```cpp
unsigned long long x1 = 1, x2 = 1, x3 = 1, x4 = 1;
for (int i = 0; i < ARRAY_SIZE; i += 4) {
    // a, b, c, d are data[i..i+3], each |1 to stay odd
    x1 = x1 * a;  x1 = x1 * (a + 2);  x1 = x1 * (a + 4);  x1 = x1 * (a + 6);
    x2 = x2 * b;  x2 = x2 * (b + 2);  x2 = x2 * (b + 4);  x2 = x2 * (b + 6);
    x3 = x3 * c;  x3 = x3 * (c + 2);  x3 = x3 * (c + 4);  x3 = x3 * (c + 6);
    x4 = x4 * d;  x4 = x4 * (d + 2);  x4 = x4 * (d + 4);  x4 = x4 * (d + 6);
}
return x1 * x2 * x3 * x4;
```

`x1`..`x4` share no dependency, so the CPU can overlap all four chains. A
second, simpler experiment in the same source (`singleAccumulator` vs.
`multipleAccumulators`, 1 vs. 8 accumulators over a plain `sum += data[i]`)
isolates the identical effect with a one-op chain instead of a four-op one.

The two are not the same shape, and the difference is worth seeing drawn:
[**Two Ways to Break a Chain**](pipeline.chains.html)
puts their dependency graphs side by side. `independentChains` is 4 lanes x 4
deep — it hides a multiply recurrence *within* one iteration.
`multipleAccumulators` is 8 lanes x 1 deep — it hides a single add *across*
iterations. That is why one needs four lanes and the other needs eight, even
though both land at ~3.8x.

**Why multiply, and why unsigned.** The whole loop is one big product, and
multiplication is associative — which is exactly what makes splitting it
into four lanes a *legal* transformation: `independentChains(data)` returns
the same value as `dependentChain(data)`, and the program prints
`Same answer? YES` to prove it. The arithmetic is unsigned because 100M
multiplies overflow immediately and signed overflow is undefined behavior;
the `| 1` keeps every factor odd, since repeatedly multiplying by even
values drives the low bits to zero and collapses the accumulator within 64
elements.

An earlier version of this example used `x = (x + data[i]) * 3 ^ (x >> 2)`
— bit-mixing that stalls just as well but is **not** associative. It made a
better-looking benchmark and a worse lesson: the "fast" version computed a
completely different answer, so it was never a transformation of the slow
one at all, just a different program with a similar cost profile.

*AI question:* _Is this the same as loop unrolling and if different how?_

## Build

```bash
clang++ -std=c++17 -O1 -o pipeline pipeline.cpp && ./pipeline
./buildandrun.sh   # sweeps -O0 through -O3
```

## Results (Apple M-series, N=100M ints, min of 5 runs)

| Level | Dependent | Independent | Speedup | Single acc | Multi acc | Speedup |
|-------|-----------|-------------|---------|-------------|-----------|---------|
| -O0   | 327 ms    | 179 ms      | 1.83x   | 105 ms      | 77 ms     | 1.36x   |
| -O1   | 269 ms    | 70 ms       | 3.84x   | 23 ms       | 6 ms      | 3.83x   |
| -O2   | 69 ms     | 70 ms       | 0.99x   | 5 ms        | 5 ms      | 1.00x   |
| -O3   | 69 ms     | 69 ms       | 1.00x   | 5 ms        | 5 ms      | 1.00x   |

Both experiments print `Same answer? YES` at every level, and the checksum
is identical across all four builds (`8981613207994990858`) — the split is
answer-preserving, not just fast.

**The gap closes at `-O2` because the compiler performs the transformation
itself.** That is the strongest possible evidence the split is legal: an
optimizer is only permitted to do this if the result is unchanged. Dumping
`-S` for `dependentChain` shows it directly — at `-O1` the loop is a strict
four-deep chain, each `mul` reading the destination of the one above:

```
mul x11, x0,  x10
mul x11, x11, x12    <- depends on the line above
mul x11, x11, x12
mul x0,  x11, x10
```

At `-O2` the same function unrolls and advances **four independent partial
products** (`x10`, `x11`, `x12`, `x13`) side by side — by hand at `-O1`,
by compiler at `-O2`:

```
mul x10, x10, x14
mul x11, x11, x15
mul x12, x12, x16
mul x13, x13, x17    <- four independent chains
```

*AI question:* _If the Apple silicon pipeline is more complex, is it benefitical to make the independent chain longer?_

*AI question:* _In an early version, you emitted the instructions not interleaved by chain (all x1 instructions than all x2 instructions) but instead interleaved by operation (x1 = x1 + data[i]; x2 = x2 + data[i + 1];) this was just as fast. Verify this is true and tell us why._


This is kind of a WOW moment about the power of modern processors. 

This is our first introduction to processor optimization levels. Let's go deeper [compiler_optimization.md](../../course_materials/01.pipeline.compiler_optimization.md)


## Verified optimization levels

Re-built and re-run at each level to confirm the table above (`clang++
-std=c++17`, same source, same machine):

| Level | Shows the effect? |
|---|---|
| `-O0` | **Yes** — 1.83x / 1.36x |
| `-O1` | **Yes** — 3.84x / 3.83x, the cleanest run |
| `-O2` | No — the compiler splits the chain itself, so both sides match |
| `-O3` | No — same as `-O2` |

**Only `-O0` and `-O1` demonstrate this example as a timing comparison** —
but for a satisfying reason. From `-O2` on the gap closes because the
optimizer applies the same transformation you just applied by hand, which
it is allowed to do precisely because multiplication is associative. The
technique doesn't stop working; it stops being *yours*.

Note the times at `-O2`/`-O3` are real (69 ms, 5 ms), not the `0 ms`
readings this file used to report. Those were a benchmark-harness artifact:
`benchmark()` did `result = func(data)`, so only the last of five calls was
ever read and the compiler deleted the other four as dead code, leaving the
minimum to latch onto a run that never happened. It now accumulates
(`result += func(data)`) instead.

## Analysis

The 1.8–3.8x speedups from breaking the dependency chain are real and
visible at -O0/-O1, where the compiler still emits something close to a
literal instruction-per-line translation. Verify it the same way every
other claim in this directory gets verified — dump `-S` and read the loop:

```bash
clang++ -std=c++17 -O1 -S -o o1.s pipeline.cpp
clang++ -std=c++17 -O2 -S -o o2.s pipeline.cpp
awk '/^__Z14dependentChain/{f=1} f{print} /ret/{if(f)exit}' o1.s | grep mul
awk '/^__Z14dependentChain/{f=1} f{print} /ret/{if(f)exit}' o2.s | grep mul
```

At `-O1` you get four multiplies in a strict chain; at `-O2`, four
independent partial products advancing in parallel (shown above). The
compiler is doing your job for you, and the fact that it *may* is the
proof that the transformation preserves the answer.

**This file has been wrong twice, in instructive ways.** Both are worth
knowing about, because both are mistakes that are easy to make in your own
measurements:

*AI observation*: _This was hallucintaion. It guessed and was wrong for a long time._

1. **It guessed at a mechanism instead of checking.** For a long time this
   file explained the `-O2`/`-O3` behavior as auto-vectorization or
   constant-folding. Nobody had dumped `-S`. When someone finally did, both
   guesses turned out to be false — the functions were byte-identical
   scalar code at every level, with no vector register anywhere.
2. **The benchmark was measuring work that never ran.** The real cause of
   the old `0 ms` readings was `benchmark()` doing `result = func(data)`:
   only the last of five calls was ever read, so the compiler legally
   deleted the other four (the same dead-store pattern as
   [pipeline.dce.md](pipeline.dce.md)), and the reported minimum came from
   a run that didn't happen. Accumulating instead of overwriting fixed it,
   and the `-O2`/`-O3` rows now show honest non-zero times.

A third correction, this one about the example rather than the
measurement: the original recurrence mixed `*` with `^` and `>>`, which is
not associative, so `independentChains` computed a *different answer* from
`dependentChain` and was never a transformation of it. Switching to a pure
product keeps the four-deep stall and makes the split legal — same answer,
verified in the program's own output.

*AI question*: _Can you use the result to prevent the compiler from optimizing it out?_

*AI answer*: Partially, and this benchmark is itself the counterexample.
Both `dependentChain` and `independentChains` already "use" their result —
they return it, `benchmark()` assigns it to `result`, and the last call's
value eventually reaches a `volatile long long dummy` at the very end. That
usage is *necessary*: it's what stops the compiler from deleting the whole
computation as unobservable dead code, the way [pipeline.dce.md](pipeline.dce.md)'s
dead-call case gets removed outright when nothing ever reads its return
value. But it is not *sufficient* to force the compiler to actually
re-execute the real work every time — using a value only obligates the
compiler to produce a value that's correct when observed, and it's still
free to get there however it can prove is equivalent and cheaper. That's
exactly the gap the paragraph above walks through: `data` never changes
across the 5 benchmarked calls, so a pure function's result is the same
every time, and "you used it" doesn't stop the optimizer from computing it
once and reusing that answer for calls whose individual results were never
separately observed (`benchmark()` only ever keeps the *minimum time* and
the *last* `result` — the other four calls' return values are simply
overwritten and never read by anything). Using the result pins down *that*
a computation must appear to have happened; it says nothing about *how many
times* or *how completely*. To force that — to stop the compiler from
noticing two calls are redundant, or from collapsing a whole loop to a
closed form the way [pipeline.dce.md](pipeline.dce.md)'s dead-store case
does — you need something stronger than "the result gets used" at the end:
a barrier at each step or each call that makes the *intermediate* value
opaque, not just the final one. That's what `clobber()` here and
`doNotOptimize()` in [pipeline.cse.cpp](pipeline.cse.cpp) are for — and
even `clobber()` isn't airtight against this, which is exactly why this
file's `-O2`/`-O3` rows used to read `0 ms`. The fix wasn't a stronger
barrier: it was making every call's result actually get read, by
accumulating in `benchmark()` instead of overwriting.

This is the same phenomenon [multiple_accs.md](multiple_accs.md)
and [../ILP/fuse_loops_rob.md](../ILP/fuse_loops_rob.md) measure far more
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
3. **Splitting a chain is only legal if the operator is associative.**
   That is the whole ballgame. A product or a sum splits into lanes and
   recombines exactly; a chain that mixes `*` with `^` and `>>` does not,
   and "parallelizing" it silently computes something else. Both
   experiments here print `Same answer? YES` — if yours doesn't, you
   haven't optimized anything, you've written a different program.
4. **When the compiler takes your optimization away, that's the proof it
   was valid.** At `-O2` the hand-split stops winning because the optimizer
   performs the same split — which it is only permitted to do because the
   answer is preserved. A transformation the compiler *can't* do for you is
   often one that isn't answer-preserving.
5. **A claim about "what the compiler did" isn't true until you've checked
   the assembly.** This file stated a vectorization/strength-reduction
   explanation for a long time without anyone dumping `-S` to confirm it —
   it sounded plausible and was wrong. Reading the generated code is one
   command; guessing about compiler internals is free and frequently
   incorrect.
6. **Check that your benchmark is measuring work that actually runs.** A
   harness that keeps only the last of N results lets the compiler delete
   the other N-1 calls outright. Accumulate every result, or time a run
   that never happened.
