# Loop Tiling, part 2 — Matrix Multiply

[loop_tiling.md](loop_tiling.md) tiles a matrix transpose and finds that padding
the leading dimension beats it. That result is real, and it is also specific to
a kernel with **no data reuse**. This is the other case — the one tiling exists
for — and here tiling wins outright and padding cannot substitute for it.

## Source

[matmul_tiling.cpp](matmul_tiling.cpp)

## The distinction that decides it

| kernel | work / data | reuse factor | what tiling can do |
|---|---|---|---|
| Transpose | N² / N² | **1** — each element touched once | only improve spatial locality |
| Matrix multiply | N³ / N² | **N** — every element of B read N times | turn N re-reads into 1 read + N−1 hits |

That is the whole difference. Transpose reads each element exactly once, so
there is no temporal reuse for a tile to capture; the only thing blocking can
fix there is a layout problem, and fixing the layout directly is cheaper.
Matrix multiply reads every element of B once per row of A, so blocking cuts
memory traffic by roughly the tile size. **No amount of padding does that.**

## Build

```bash
g++ -O3 -march=native -std=c++17 -o matmul_tiling matmul_tiling.cpp
./matmul_tiling          # tile size defaults to 256
./matmul_tiling 128      # or pass one
```

> **Build at `-O3`, not `-O1` or `-O2`.** This example is the exception to the
> directory's usual `-O1`, and the reason is worth knowing — see
> [the optimisation-level trap](#the-optimisation-level-trap) below. At `-O2`
> it reports that tiling does nothing.

## Results

Baseline is `i,k,j` — loop interchange already applied, so everything below is
what **blocking** buys on top of the best non-blocked loop order. Tile size 256,
pinned to one core with the clock warmed.

| n | total | i,k,j | + block j | + block i,j | + block i,j,k |
|---|---|---:|---:|---:|---:|
| 512 | 6 MB — fits L3 | 27.2 GF/s | 29.5 | 30.0 | **34.5** |
| 1024 | 24 MB | 26.5 GF/s | 22.8 | 22.7 | **31.9** |
| 2048 | 96 MB | 10.7 GF/s | 22.0 | 22.9 | **31.8** |

As speedups against `i,k,j`:

| n | + block j | + block i,j | + block i,j,k |
|---|---:|---:|---:|
| 512 | 1.08x | 1.10x | 1.27x |
| 1024 | 0.86x | 0.86x | 1.20x |
| **2048** | **2.06x** | **2.14x** | **2.98x** |

## Analysis

### The speedup grows with the problem

Read the fully-blocked column down the page: **31.8, 31.9, 34.5 GFLOP/s** — the
blocked kernel runs at the same rate regardless of size. It is compute-bound,
and the problem size has stopped mattering.

Now read the baseline column: **27.2, 26.5, 10.7**. It keeps up while the
matrices fit in L3 and falls off a cliff when they do not, because it re-reads
all of B once per row of A. At n=2048 that is 2048 passes over a 32 MB matrix —
about 69 GB of traffic, against a measured DRAM ceiling near 39 GB/s.

So tiling here is not a constant-factor tweak. It changes the *amount of memory
traffic the algorithm generates*, from O(N³) down to O(N³/T). The speedup is
1.27x when everything fits and 2.98x when it does not, and it would keep growing
with n.

### Blocking one dimension: a teaching step, not a shortcut

`block j` is a single extra loop on top of the interchanged kernel — the
smallest change that shows the idea — and at n=2048 it captures about two thirds
of the win.

But look at n=1024, where it **loses** (0.86x). The working set only just
exceeds L3 there, and one dimension of blocking adds loop overhead without
cutting enough traffic to pay for it. Only blocking all three helps.

Start with `block j` to understand the transformation. Do not ship it.

### The tile loop order is not free either

In the three-dimensional version, `kk` is the **innermost** of the three tile
loops:

```c
for (ii …) for (jj …) for (kk …)     // <- kk inside
```

so the C tile is loaded once and accumulated into across the whole `kk` sweep.
Ordering `kk` outside `jj` re-reads the C tile n/T times and throws most of the
benefit away. This is the same class of mistake as the original loop order the
example starts from — a blocked loop nest has its own interchange question.

### The optimisation-level trap

The inner loop is `c[j] += a * b[j]`. Two things have to be true before it
vectorises, and if either fails, every version in the table runs scalar at about
1.3 flops/cycle — which makes the whole kernel memory-bound and leaves tiling
nothing to win:

1. **`__restrict` on the pointers.** Without it the compiler cannot rule out `c`
   and `b` aliasing, and declines.
2. **`-O3`.** GCC's `-O2` uses the `very-cheap` vectoriser cost model, which
   rejects loops with runtime trip counts like these.

Measured on this source at n=2048: at `-O2` every blocked version lands within
**4%** of the unblocked one — the example appears to show that tiling is
worthless. At `-O3` the identical code gives **2.98x**.

This is worth dwelling on, because it is how a correct experiment produces a
false conclusion. Anyone measuring blocked matmul at `-O2` will find that
blocking does not work, write it up, and be wrong.

## Key takeaways

1. **Tiling exploits reuse.** Before reaching for it, ask what the reuse factor
   is. Transpose's is 1, and no tile size will change that. Matrix multiply's is
   N, which is why it is the canonical example rather than an arbitrary one.

2. **Tiling and conflict-avoidance are independent.** Padding the leading
   dimension helps the *blocked* version at small tile sizes and does nothing
   for the unblocked one. Tiling wins ~3x with or without padding. They fix
   different problems and compose.

3. **The win grows with the problem.** A tiled kernel is size-insensitive; an
   untiled one degrades once the working set passes the last cache level. Measure
   at a size that actually exceeds cache or you will measure nothing.

4. **Check that your kernel vectorised before concluding anything about
   memory.** A scalar inner loop is slow enough to hide every memory effect you
   were trying to study.
