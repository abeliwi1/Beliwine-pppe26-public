# Parallel Prefix Sum (Inclusive Scan)

The example behind [Lecture 12](../../course_materials/12.prefixsum.md). A
prefix sum's serial loop can't be parallelized by adding a pragma: every
element depends on the one before it. The parallel version is a different
algorithm, and it does more work than the serial one to get its parallelism.

## What is a prefix sum?

Given an input array `A[0..N-1]`, the **inclusive prefix sum** (scan) produces an output array `B` where:

```
B[0] = A[0]
B[1] = A[0] + A[1]
B[2] = A[0] + A[1] + A[2]
  ...
B[i] = A[0] + A[1] + ... + A[i]
```

Prefix sums appear everywhere: cumulative histograms, stream compaction, radix sort, polynomial evaluation, and load balancing in parallel algorithms.

## Why the naive parallel approach fails

The straightforward serial loop has a **carried dependency** — each output element depends on the previous:

```c
out[i] = out[i-1] + in[i];   // can't parallelize directly
```

Splitting this across threads and running `#pragma omp parallel for` produces a **race condition**: thread 1 may read `out[chunk-1]` before thread 0 has written it.

## Implementation strategy: block parallelism

This implementation uses **block parallelism**: each thread owns a single
contiguous slice of the array.

```
Thread 0: [0 .. N/T-1]
Thread 1: [N/T .. 2N/T-1]
Thread 2: [2N/T .. 3N/T-1]
  ...
```

Contiguous blocks are **required**. Each thread must prefix-sum its own block
independently in pass 1, before the offsets are known, which only works if
its elements are consecutive. And passes 1 and 3 must use exactly the same
blocks, because pass 3 adds the offset computed from pass 1's totals.
OpenMP's default `static` schedule would also give each thread one contiguous
block, but the code computes the blocks itself from `omp_get_thread_num()`, so
the block boundaries and the per-block totals line up across the passes by
construction.

## The three-pass parallel algorithm

The solution breaks the computation into three phases:

### Pass 1 — Local prefix sum (parallel)

Each thread independently prefix-sums its own contiguous chunk. This is fully parallel with no inter-thread communication.

```
Thread 0: [a0  a0+a1  a0+a1+a2  ...]    total0 = sum of chunk 0
Thread 1: [b0  b0+b1  b0+b1+b2  ...]    total1 = sum of chunk 1
Thread 2: [c0  c0+c1  c0+c1+c2  ...]    total2 = sum of chunk 2
```

### Pass 2 — Prefix sum of chunk totals (serial)

Compute the prefix sum of the `T` per-thread totals (where T is the number of threads — tiny compared to N):

```
offset[0] = 0
offset[1] = total0
offset[2] = total0 + total1
...
```

This runs serially over T elements, so it is negligible.

### Pass 3 — Add offsets (parallel)

Each thread (except thread 0) adds its offset to every element in its chunk:

```c
for (int i = start; i < end; i++)
    out[i] += offset[tid];
```

Again fully parallel and independent across threads.

### Diagram

```
Input:   [ a b c | d e f | g h i ]
                  T0      T1      T2

Pass 1 (parallel):
         [a a+b a+b+c | d d+e d+e+f | g g+h g+h+i]
          totals: [a+b+c, d+e+f, g+h+i]

Pass 2 (serial, 3 ops):
          offsets: [0, a+b+c, a+b+c+d+e+f]

Pass 3 (parallel):
         [a a+b a+b+c | a+b+c+d a+b+c+d+e ... | ...]
                           ^ added offset[1]
```

## Two parallel versions

`omp_prefix_sum.c` has two parallel versions with the same three-pass
structure, one contiguous block per thread:

| | pass 1 (parallel) | pass 2 (serial) | pass 3 (parallel) | memory traffic |
|---|---|---|---|---|
| **scan, then add** (`prefix_omp`) | scan each block of `in` into `out`; keep its total | offsets from the totals | add the offset to each block of `out` | read `in`, write `out`, read and write `out` again: **4 × 800 MB** |
| **reduce, then scan** (`prefix_omp_reduce_scan`) | sum each block of `in`; write nothing | offsets from the sums | scan each block of `in` into `out`, starting from its offset | read `in`, read `in` again, write `out`: **3 × 800 MB** |

Pass 1 of reduce-then-scan is a plain sum, which can be added up in any order,
so `#pragma omp simd reduction(+:s)` lets the compiler use vector registers and
several partial sums. A scan can't be reordered that way.

## Performance results

Apple M5 (4 performance + 6 efficiency cores), N = 100,000,000 doubles
(800 MB per array), fastest of four trials, two runs each. Every parallel result
matches the serial one (relative error below 10⁻¹⁰).

| threads | serial | scan, then add | reduce, then scan |
|---|---|---|---|
| 1 | 51 ms | 51 ms (1.0×) | 62 ms (0.83×) |
| 2 | 51 ms | 33 ms (1.6×) | 36 ms (1.4×) |
| 4 | 51 ms | 22.5–24.5 ms (2.1–2.3×) | 23–24 ms (2.2×) |
| 10 | 52–54 ms | 24–25 ms (2.1×) | 19.4–20.3 ms (2.6×) |

Memory bandwidth for reference: `examples/openmp/dot_product` reads two arrays
at about 120 GB/s with 10 threads.

### What limits each one

- **Serial is limited by its chain of adds, not by memory.** Each element waits
  for the one before it, about 0.5 ns per element, and it moves only 31 GB/s.
- **Parallel versions do more work.** Every element is added twice (once in
  pass 1, once in pass 3), and the arrays are streamed three or four times
  instead of twice. On one thread that makes reduce-then-scan *slower* than
  serial (62 ms). Scan-then-add breaks even on one thread only because thread 0
  skips pass 3.
- **With a few threads, the chains dominate.** Each thread's pass-1 scan is
  still a chain of N/T dependent adds. Scan-then-add has one chain per element,
  and reduce-then-scan's chain is in pass 3, so they are about equal at 4
  threads.
- **With 10 threads, memory bandwidth is the limit.** Scan-then-add moves
  3.2 GB in about 24 ms, and reduce-then-scan moves 2.4 GB in about 20 ms:
  both about 120–130 GB/s, the same ceiling the dot product reaches. So the
  version that moves fewer bytes wins, and neither gets far past 2.5×, however
  many cores there are.

Amdahl's law isn't the limit: pass 2 is T additions out of hundreds of
millions. The limit is that the parallel algorithm moves more data than the
serial one, through a memory system the cores share.

### The Cilk versions

`cilk_prefix_sum.c` implements scan-then-add with `cilk_for`, plus a variant
that does passes 1 and 3 in a single `cilk_for` body, separated by a spinning
software barrier. Measured with the same N, blocks = workers
(`CILK_NWORKERS=W ./cilk_prefix_sum 100000000 W`), fastest of four trials, two
runs:

| workers | serial | `cilk_for` | `cilk_for` + barrier |
|---|---|---|---|
| 1 | 51 ms | 50.2 ms (1.0×) | 50.2–52.4 ms (1.0×) |
| 2 | 51 ms | 32.4–33.0 ms (1.6×) | 33.8–34.1 ms (1.5×) |
| 4 | 51 ms | 22.3–22.5 ms (2.3×) | 22.8–23.0 ms (2.2×) |
| 10 | 51–52 ms | 23.9 ms (2.15×) | 27.0–27.3 ms (1.9×) |

`cilk_for` matches OpenMP's scan-then-add at every worker count. Before a
one-line change it didn't: 221.5 ms on 1 worker (4.3× slower than serial),
119 ms on 2, 68 ms on 4 and 39–41 ms on 10.

**Why: the compiler.** Pass 1 was written like the serial loop,
`out[i] = out[i-1] + in[i]`. In the serial function and in OpenMP's outlined
region, the compiler keeps `out[i-1]` in a register, so each element is a load,
an add and a store, about 0.5 ns. Inside the `cilk_for` body it didn't: it
reloaded `out[i-1]` from memory on every iteration, so each addition waited for
the previous iteration's store to come back, about 2.2 ns per element.
Carrying the running sum in a local variable (`s += in[i]; out[i] = s;`) gives
the same three-instruction loop as serial. Compare the inner loops with
`~/opencilk/bin/clang -fopencilk -O3 -S cilk_prefix_sum.c`.

The barrier variant spins, so it works only if every block runs at the same
time on its own worker. The code caps the block count at the worker count and
sets `grainsize(1)` so each block is its own task. Without those guards it
hangs (for example, 10 blocks with `CILK_NWORKERS=8`).

## Floating-point note

The parallel version produces results that are **not bit-identical** to the serial version because floating-point addition is not associative. Reordering the additions (by chunk) changes the rounding at each step. Both results are correct to within floating-point precision; the relative error is typically < 1e-10.

## Building and running

```bash
make                                # builds omp_prefix_sum and cilk_prefix_sum
./omp_prefix_sum [N] [threads]      # defaults: N=100000000, threads=10
./cilk_prefix_sum [N] [blocks]      # defaults: N=100000000, blocks=10

# Scaling chart of scan-then-add (needs numpy, scipy, matplotlib)
python3 prefixsum_chart.py
```
