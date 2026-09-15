# Speculative Execution Example

Demonstrates the cost of branch misprediction by running the same conditional
loop over sorted vs randomly shuffled data, isolating the effect of speculation
accuracy on pipeline performance.

## Source

[speculative_execution.cpp](speculative_execution.cpp)

## What is speculative execution?

When a CPU reaches a conditional branch, it does not know the outcome until the
condition has been evaluated — which may be several cycles later due to pipeline
depth.  Rather than stalling, the CPU **predicts** the branch direction and
continues fetching and executing instructions along the predicted path.  This is
speculative execution.

- **Correct prediction:** the speculative work commits; the pipeline runs at
  full utilization as if the branch did not exist.
- **Wrong prediction:** the pipeline is flushed back to the branch point; all
  instructions issued along the wrong path are discarded.  On Apple M5 this
  flush costs approximately **15 cycles**.

The branch predictor learns patterns from recent branch history.  Branches that
follow a predictable pattern (always taken, never taken, alternating) are
predicted with near-perfect accuracy.  Random or data-dependent branches with
no repeating structure approach 50% accuracy — equivalent to a coin flip.

---

## How it works

Both versions of the branchy loop execute **identical instructions** on
**identical data values**.  Only the order in which elements are visited differs.

**Shuffled data — unpredictable branch:**

```cpp
if (data[i] >= THRESHOLD)   // data in random order
    sum += data[i];
```

Values are uniformly distributed in `[0, 255]` with `THRESHOLD = 128`.
Approximately 50% of branches are taken, in no repeating pattern.  The
predictor is correct ~50% of the time.

Expected overhead per element: `0.5 mispredict × 15 cycles = 7.5 extra cycles`

**Sorted data — predictable branch:**

Same code, same values, sorted ascending.  All elements below threshold appear
first; all elements above appear last.  The predictor learns "not taken" for the
first half and "taken" for the second, mispredicting exactly once at the
transition.  The 15-cycle penalty is paid once across all N elements.

**Branchless — no branch instruction:**

```cpp
sum += (int64_t)v * (v >= THRESHOLD);
```

The boolean `(v >= THRESHOLD)` evaluates to 0 or 1.  Multiplying by `v` gives
0 or `v` with no branch instruction in the generated code — no prediction, no
speculation, no flush possible.  Performance is independent of data order.

---

## Compiler note

Apple Clang converts the `if`-statement to `csel` (conditional select) even at
`-O1`, eliminating the branch instruction before we can observe its cost.

It does this *thoroughly*.  At plain `clang++ -O1`, `sum_conditional` and
`sum_branchless` compile to the same inner loop, differing only in how the
addend is widened:

```
; sum_conditional @ clang -O1          ; sum_branchless @ clang -O1
ldr  w10, [x0], #4                     ldr  w10, [x0], #4
cmp  w10, #127                         cmp  w10, #127
csel w10, w10, wzr, gt   ← no branch   csel w10, w10, wzr, gt
add  x8, x8, x10                       add  x8, x8, w10, sxtw
subs x9, x9, #1                        subs x9, x9, #1
b.ne .loop                             b.ne .loop
```

Built that way, all three rows of this benchmark time identical code and report
~1.00x across the board.  **The example measures nothing unless a real branch is
preserved deliberately.**

### Preserving the branch

Two `-mllvm` flags keep the branch in `sum_conditional` while leaving
`sum_branchless` branch-free — which is exactly the contrast the example needs:

```bash
clang++ -O1 -mllvm -two-entry-phi-node-folding-threshold=0 \
        -mllvm -aarch64-enable-early-ifcvt=false \
        -o spec_O1 speculative_execution.cpp && ./spec_O1
```

The first stops SimplifyCFG from folding the two-entry phi into a select; the
second stops the AArch64 backend from re-forming a `csel` afterward.  Both are
needed — either alone still yields `csel`.  Verifying the result:

```
; sum_conditional, both flags: real branch
LBB0_3:
    ldr  w10, [x0], #4
    cmp  w10, #128
    b.lt LBB0_2          ← data-dependent conditional branch — mispredictable
    add  x8, x8, x10
LBB0_2:
    subs x9, x9, #1
    b.eq LBB0_6
```

`sum_branchless` still compiles to `csel` under these flags, as intended.

On a machine with real GCC, `-fno-if-conversion` does the same job:

```bash
g++-15 -O1 -fno-if-conversion -o spec_O1 speculative_execution.cpp && ./spec_O1
```

Note that Apple's `g++` is a Clang shim, not GCC — `g++ --version` on macOS
prints "Apple clang version".  The GCC recipe requires a genuinely separate
install (Homebrew `gcc`), and the flag is silently unavailable otherwise.

The fact that Clang reaches for `csel` unprompted is itself the lesson: the
compiler applies the same branchless transformation a programmer would apply
manually, eliminating the misprediction penalty with no source change — which
is why we have to fight it to *show* you the penalty at all.

---

## Inner loop instruction count

From the GCC `-O1 -fno-if-conversion` assembly, the inner loop has:

| path | instructions |
|------|-------------|
| not taken (`data[i] < THRESHOLD`) | ldr, cmp, ble, add(ptr), cmp(ptr), beq = **6** |
| taken (`data[i] >= THRESHOLD`) | same 6 + add(sum) + b(loop) = **8** |
| average at 50% taken | **7 insns/elem** |

The instruction count is identical across all three benchmark versions —
sorted, shuffled, and branchless — so speedup equals the CPI ratio directly.

---

## Results (Apple M5, N=32M ints, 128 MB, threshold=128, Apple Clang 17)

Built with the two `-mllvm` flags above:

| version | time | cycles/elem | CPI | speedup |
|---------|------|-------------|-----|---------|
| branchy + shuffled (50% mispredict) | 83.1 ms | 9.90 | 1.41 | 1.00x |
| branchy + sorted (1 mispredict) | 7.7 ms | 0.91 | 0.13 | **10.85x** |
| branchless + shuffled (no branch) | 8.4 ms | 1.00 | 0.14 | **9.92x** |

Expected misprediction overhead: `32M × 0.5 × 15 cycles / 4 GHz ≈ 60 ms` added
to the base 7.7 ms gives ~68 ms predicted vs 83.1 ms measured — close, with the
remainder attributable to pipeline refill after each flush.

> **Branchless is not quite free.**  At sub-millisecond resolution the branchless
> version is measurably *slower* than the sorted branchy one — 8.4 vs 7.7 ms —
> because `csel` unconditionally performs the select and feeds it into the
> accumulator every iteration, where the sorted branchy version simply skips
> the add on half its elements.  Branchless trades a guaranteed small cost for
> an unpredictable large one.  That is a good trade on random data and a losing
> one on sorted data.  

---

## Correlation: speedup = CPI ratio

Because instruction count is the same for all three versions, time is
proportional to CPI, and speedup equals the CPI ratio exactly:

```
speedup = CPI_shuffled / CPI_sorted = 1.41 / 0.13 = 10.8x  [measured: 10.8x]
```

This identity is exact for the shuffled-vs-sorted pair and only that pair: those
two runs execute *the same binary over the same values*, differing solely in the
order the values are visited.  Instruction count is therefore identical by
construction, and every bit of the 10.85x is CPI — that is, stall cycles.

The branchless row is a genuinely different instruction mix (`csel` in place of
a conditional branch, 6 instructions per element rather than 6-or-8), so its
speedup is *not* a pure CPI ratio and the identity does not apply to it.

The 0.13 CPI of the sorted version (well under 1) reflects the M5's wide
superscalar OOO engine retiring several instructions per cycle across multiple
in-flight iterations once no branch stalls occur.

---

## Key takeaways

1. **Branch misprediction serializes the pipeline.** Each wrong guess discards
   15+ cycles of speculative work and re-fetches from the correct path.  At 50%
   misprediction, roughly half the CPU's time is wasted on discarded work.

2. **Data order determines prediction accuracy.** The same code, the same
   values, the same instruction count — only memory layout differs.  Sorted
   data gives 10x better performance than random data on this benchmark.

3. **Branchless code avoids the problem, at a price.** Replacing a conditional
   branch with arithmetic (`v * (v >= threshold)`) removes the prediction
   problem at the cost of always doing the select.  On random data that is a
   huge win (9.92x); against *sorted* data it is a small loss (8.4 vs 7.7 ms),
   because a well-predicted branch is nearly free while the select is never
   free.  Branchless converts an unpredictable large cost into a guaranteed
   small one — worth it exactly when the branch is unpredictable.

4. **Modern compilers apply branchless transformation automatically.** Apple
   Clang at `-O1` emits `csel` for the simple `if` — so completely that
   `sum_conditional` and `sum_branchless` become the same loop.  Exposing the
   raw hardware effect requires actively defeating this
   (`-mllvm -two-entry-phi-node-folding-threshold=0
   -mllvm -aarch64-enable-early-ifcvt=false`, or GCC's `-fno-if-conversion`).
   When your benchmark and its control compile to identical code, you are
   measuring nothing — always check.

5. **Speedup = CPI ratio when instruction count is constant.** Unlike the
   out-of-order accumulator example (where both CPI and instruction count
   change), here only CPI changes — making the speedup a direct, unambiguous
   measure of the misprediction cost.
