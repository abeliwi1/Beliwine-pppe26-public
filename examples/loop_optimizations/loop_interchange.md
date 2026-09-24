# Loop Interchange

Reordering nested loop indices to convert stride-N column access into stride-1
row access, for matrix-vector multiplication. A pure reordering — same
arithmetic, same results — worth **6.3x**.

## Source

[loop_interchange.cpp](loop_interchange.cpp)

## Machine

AMD Ryzen AI 9 HX 370 (Zen 5, "Strix Point"), Linux, g++ 13.3. L1d **48 KB**
per core, 12-way, **64-byte lines** (8 doubles per line); L2 1 MB per core;
L3 16 MB. Benchmarks pinned to a 5.13 GHz core, clock warmed before measuring.

## What is loop interchange?

Loop interchange swaps the order of two nested loops. When the body accesses a
multi-dimensional array, the index order determines the memory stride. In
row-major storage the rightmost index is contiguous; if the inner loop
increments a non-rightmost index, each iteration steps a full row — thousands
of bytes — and misses on every access. Swapping the loops so the inner loop
increments the rightmost index restores stride-1 access with no algorithmic
change.

## How it works

Computing `y = A·x` where A is M×N row-major, x is N×1.

**j,i order — column access of A (cache-unfriendly):**

```cpp
for (int j = 0; j < N; j++) {
    double xj = x[j];
    for (int i = 0; i < M; i++)
        y[i] += A[i][j] * xj;
}
```

The inner loop increments `i`, stepping down column `j`. In row-major storage
`A[i][j] = A[i*N + j]`, so consecutive `i` are `N` doubles apart — a stride of
`N × 8 = 32 KB`. Every access to A is a miss, and each miss drags in a
64-byte line of which exactly 8 bytes are used.

**i,j order — row access of A (cache-friendly):**

```cpp
for (int i = 0; i < M; i++) {
    double acc = 0.0;
    for (int j = 0; j < N; j++)
        acc += A[i][j] * x[j];
    y[i] = acc;
}
```

The inner loop increments `j`, scanning row `i` sequentially — stride 8 bytes,
so one 64-byte line serves 8 consecutive elements and 7 of every 8 accesses are
free. `y[i]` is promoted to a scalar accumulator held in a register for the
whole inner loop.

[loop_interchange_memory.html](loop_interchange_memory.html) draws the two
orders side by side: one 8x8 array shown both as the grid you picture and as the
flat run of memory it actually occupies, with the 64-byte cache lines marked.
Step either traversal and watch the tally — across a row, one fetched line
answers all eight reads; down a column, eight lines come in and seven eighths of
each is thrown away untouched. Standalone; double-click it.

## Build

```bash
g++ -O1 -o interchange_O1 loop_interchange.cpp && ./interchange_O1
```

## Results

M = N = 4096, A = 128 MB.

| Version | Access pattern | Time | Speedup |
|---|---|---:|---:|
| j,i order | stride-32KB column access, one miss per element | 67.2 ms | baseline |
| i,j order | stride-1 row access, `x` resident in L1 | 10.7 ms | **6.29x** |

Access pattern summary, as the program prints it:

```
A row (one inner loop pass):  32 KB
x vector:                     32 KB
must stay resident (x only):  32 KB   (fits L1)   [L1d = 48 KB, 64-byte line]
doubles per cache line:       8       (stride-1 amortises one miss over 8)
j,i inner stride on A:        32 KB per step      (one miss per element)
```

## What actually has to fit in cache

The row of A is **32 KB** and the `x` vector is **32 KB**, against a 48 KB L1d.
They do not both fit — and they do not need to.

The row of A is read once, in order, and never revisited. It streams through
the cache; retaining it would buy nothing. `x`, by contrast, is re-read in full
for every one of the 4096 rows, so it is the only thing that has to stay
resident. 32 KB against 48 KB is a comfortable fit with room to spare.

This distinction is worth making explicitly, because the tempting version of
the argument — add up everything the inner loop touches and compare it to the
cache size — gives 64 KB against a 48 KB L1 and predicts the optimisation
shouldn't work. It works fine. **Count what must be retained, not what is
touched.**

## Analysis

### Where the 6.3x comes from

Two multiplicative effects, not one:

1. **Line utilisation.** Stride-1 access uses all 8 doubles in every 64-byte
   line it pulls in. Stride-32KB access uses 1 of 8 and discards the rest. That
   is 8x more memory traffic for the same arithmetic.

2. **Prefetchability.** The stride-1 stream is trivially predicted and the
   prefetcher runs ahead of it. The 32 KB stride is a constant stride the
   prefetcher can in principle recognise, but each prefetch targets a distinct
   line that is evicted before it is reused — there is nothing useful to fetch
   ahead of time.

The measured 6.3x lands slightly below the 8x line-utilisation bound, which is
about right once the DRAM-side sequential bandwidth advantage and the
non-trivial cost of the arithmetic itself are accounted for.

### It is a pure reordering

The two versions compute identical results — the correctness check reports
`max_err = 0`, not a small tolerance. No work is eliminated, no precision is
traded away, no algorithm is changed. All 6.3x comes from the order in which
the same loads are issued.

That is what makes interchange attractive when it applies: there is no
downside to weigh, only a legality question.

### When it is legal

Interchange is valid when the loop iterations are independent — when
iteration `(i,j)` does not depend on a value written by an iteration that
would move after it in the new order. Matrix-vector multiply qualifies because
each `y[i]` accumulates from a disjoint set of inputs. A loop carrying a
dependence across the interchanged indices — `A[i][j] = A[i-1][j+1] + 1`, say —
does not.

## Measurement notes

Two machine properties will corrupt these numbers if ignored, and both are
handled in the source:

1. **Heterogeneous cores.** cpu0–3 are Zen 5 at 5.13 GHz, cpu4–11 are Zen 5c at
   3.17 GHz. An unpinned run lands wherever the scheduler puts it — a 1.6x
   spread from the same binary. The benchmark pins to cpu0.

2. **The clock only boosts on high IPC.** `amd-pstate-epp` raises the clock in
   response to instructions-per-cycle, not to the core being busy. Both loops
   here are memory-bound and low-IPC, so left alone they run at ~3.59 GHz and
   report ~94 ms / ~14 ms instead of 67 / 11. Min-of-N does not help, because
   every repetition is equally slow. The benchmark warms the core on a high-IPC
   loop first and prints the clock at both ends of the run.

Note that the *ratio* survives both traps — it is the absolute times that move.
Speedups are more robust to this class of error than timings are, which is a
reason to report both.

## Key takeaways

1. **The inner loop index should be the rightmost array index.** In row-major
   storage that is the only way to get stride-1 access; any other ordering
   strides by at least a full row.

2. **Count what must be retained, not what is touched.** A streamed row does
   not need to fit in cache. Here only `x` does, and the naive
   everything-touched total would have predicted failure.

3. **The prefetcher cannot rescue a large stride.** It recognises the pattern
   but every line it fetches is evicted before use.

4. **Interchange is free when it is legal.** Identical results to the last bit,
   6.3x faster, no trade-off — just a dependence check.
