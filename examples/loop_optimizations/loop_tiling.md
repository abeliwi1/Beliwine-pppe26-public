# Loop Tiling

Loop tiling (loop blocking) applied to a matrix transpose — a kernel with **no
data reuse**, where it turns out that padding the leading dimension beats every
tile size without restructuring the loop at all.

That result is specific to this kernel, and the reason is worth stating up
front: a transpose reads every element exactly once, so there is no temporal
reuse for a tile to capture. For the case tiling actually exists for — a kernel
that reads the same data many times — see
[matmul_tiling.md](matmul_tiling.md), where blocking wins ~3x and padding
cannot substitute for it.

**Start with the picture.**
[tiling_iteration_order.html](tiling_iteration_order.html) walks a 16&times;16
iteration space two ways on one clock — row by row, and in 4&times;4 tiles —
with the row and column operands drawn along the edges so you can see how much
has to stay in cache during each. Over one unit of work the unblocked walk
needs 1 row + 16 columns live; the blocked walk needs 4 + 4. That is the
transformation, before any of the numbers below. Standalone; double-click it.

## Source

[loop_tiling.cpp](loop_tiling.cpp)

## Machine

AMD Ryzen AI 9 HX 370 (Zen 5, "Strix Point"), Linux, g++ 13.3.

| | |
|---|---|
| L1d | 48 KB per core, **12-way**, **64 sets**, **64-byte lines** |
| L2 | 1 MB per core |
| L3 | 16 MB shared by the four Zen 5 cores |
| core clock | 5.13 GHz (cpu0–3) |

The associativity is measured, not looked up: a pointer chase over `W` lines
that all map to one set is flat at 1.12 ns through `W=12` and jumps 3.5x to
3.9 ns at `W=13`. The line size comes from a stride sweep — cost per access
grows linearly to stride 64 and is flat from 64 to 256.

## What is loop tiling?

Tiling restructures a loop's iteration space into rectangular blocks so the
working set of each block fits in a fast cache level. Without it, data may be
loaded from DRAM O(N) times as the cache evicts it before reuse; with it, data
is loaded once per block and reused as many times as the block allows.

## Why transpose

Matrix transpose has a miss pattern the hardware prefetcher **cannot** fix:

- Reading `in[i][j]` with `j` incrementing — stride-1, prefetchable.
- Writing `out[j][i]` with `j` incrementing — stride `lda`, a different cache
  line every time, and nothing for a stride-1 prefetcher to work with.

Transpose is a clean way to *isolate* blocking, because it has no useful loop
reordering available — whatever tiling buys is tiling's. What it cannot show is
what tiling is normally for, since it has no reuse. Matrix multiply does have a
loop-order question tangled up with the blocking one, but that is handled by
measuring against an already-interchanged `i,k,j` baseline rather than by
avoiding the kernel; see [matmul_tiling.md](matmul_tiling.md).

## How it works

**Naive — reads stride-1, writes stride-`lda`:**

```cpp
for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++)
        out[j*lda + i] = in[i*lda + j];   // write jumps a full row per step
```

**Tiled — B×B blocks:**

```cpp
for (int ii = 0; ii < N; ii += B)
for (int jj = 0; jj < N; jj += B) {
    int ilim = min(ii + B, N), jlim = min(jj + B, N);
    for (int i = ii; i < ilim; i++)
    for (int j = jj; j < jlim; j++)
        out[j*lda + i] = in[i*lda + j];
}
```

Within a tile the writes land in B rows of one B×B block of the output, so
those rows can stay in L1 for the duration of the tile and each output line is
written several times before eviction.

Both kernels take an explicit leading dimension `lda`, which is what makes the
second experiment below a single-variable change.

## Build

```bash
g++ -O1 -o tiling_O1 loop_tiling.cpp && ./tiling_O1
```

## Results

### Table 1 — tile size, at the natural `lda = N = 4096`

4096×4096 doubles, 128 MB per matrix.

| tile | work. set | fits L1 | time | speedup |
|---|---|---|---:|---:|
| naive | — | — | 188.4 ms | 1.00x |
| 2 | 0 KB | yes | 109.3 ms | 1.72x |
| 4 | 0 KB | yes | 62.4 ms | 3.02x |
| 8 | 1 KB | yes | 40.1 ms | 4.70x |
| 10 | 1 KB | yes | 42.0 ms | 4.48x |
| 12 | 2 KB | yes | 36.8 ms | 5.12x |
| 14 | 3 KB | yes | 34.7 ms | 5.43x |
| **16** | **4 KB** | **yes** | **34.2 ms** | **5.51x** |
| 18 | 5 KB | yes | 38.9 ms | 4.84x |
| 20 | 6 KB | yes | 44.9 ms | 4.19x |
| 24 | 9 KB | yes | 58.2 ms | 3.24x |
| 32 | 16 KB | yes | 74.9 ms | 2.51x |
| 48 | 36 KB | yes | 116.4 ms | 1.62x |
| 64 | 64 KB | no | 144.9 ms | 1.30x |
| 80 | 100 KB | no | 175.6 ms | 1.07x |
| 128 | 256 KB | no | 196.0 ms | 0.94x |
| 256 | 1024 KB | no | 194.2 ms | 0.95x |

### Table 2 — leading dimension, same N=4096 transpose

Nothing changes but the row stride. The loops, the logical matrix size, and
the element count are identical.

| lda | row bytes | L1 sets a column reaches | naive | tiled B=16 | naive vs lda=4096 |
|---|---:|---:|---:|---:|---:|
| 4096 | 32768 | **1** | 188.7 ms | 34.8 ms | 1.0x |
| 4097 | 32776 | *fractional* | 29.2 ms | 33.4 ms | 6.5x |
| **4104** | 32832 | **64** | **24.1 ms** | 35.0 ms | **7.8x** |
| 4112 | 32896 | 32 | 26.6 ms | 34.6 ms | 7.1x |
| 4128 | 33024 | 16 | 23.5 ms | 34.5 ms | 8.0x |
| 4160 | 33280 | 8 | 40.0 ms | 38.3 ms | 4.7x |
| 4224 | 33792 | 4 | 61.0 ms | 39.0 ms | 3.1x |
| 4352 | 34816 | 2 | 73.2 ms | 38.8 ms | 2.6x |

## Analysis

### The set-conflict mechanism

A line's L1 set here is `(address / 64) % 64`. Walking down a column steps one
row = `lda × 8` bytes = `lda / 8` lines, so the set index advances by
`(lda / 8) % 64` per step, and a column reaches

```
sets_reachable = 64 / gcd((lda / 8) % 64, 64)
```

distinct sets. At `lda = 4096` that advance is **0**: every element of a column
lands in **one** set. That set has 12 ways, so a 4096-element column evicts
itself continuously — which is the entire reason the naive transpose costs
188 ms instead of 24 ms.

Table 2 is the controlled version of that claim. The naive time tracks
`sets_reachable` monotonically, and collapses once a column reaches **16**
sets. Sixteen is not arbitrary: 16 sets × 12 ways is enough to hold the
column's working set. The same threshold appears in
[`set_conflict.c`](../memory_hierarchy/set_conflict.c) in the memory hierarchy
directory, reached with a completely different kernel.

### Padding beats tiling

The best tile size on the unpadded array gets to 34.2 ms. The naive loop on a
padded array gets to **23.5 ms** — 1.4x faster than the best tiling ever
achieves, with no change to the loop at all.

And once the stride is padded, tiling stops helping and starts *hurting*: the
B=16 column in Table 2 is flat at ~35 ms while the naive column drops to 24 ms.
Tiling is a way to cope with a bad leading dimension. Fixing the leading
dimension is a way not to have one.

**For a kernel with no reuse, the order of operations is the reverse of the
usual advice: pad first, and reach for tiling only if the problem still doesn't
fit.** This is why BLAS and NumPy pad array dimensions.

The qualifier matters. Where there *is* reuse, tiling is the primary
transformation and padding is a secondary cleanup on top of it —
[matmul_tiling.md](matmul_tiling.md) measures both and finds them independent:
blocking wins ~3x with or without padding, and padding helps only the blocked
version. The question to ask first is not "tile or pad" but **"how many times
does this kernel read the same byte?"**

### Neither rule picks the tile

For the record, on the unpadded array:

- **The capacity rule**, `2 × B² × 8 ≤ 48 KB`, predicts `B ≤ 55`. Badly wrong —
  B=48 and B=64 are near the bottom of Table 1.
- **The associativity rule**, `B ≤ 12`, at least lands in the right
  neighbourhood, but the measured optimum is B=16 and the rule does not pick
  it. Associativity also varies by machine, so the rule does not transfer.

The curve is not even smooth: **B=10 is reproducibly worse than B=8**, because
8 doubles is exactly one 64-byte line and 10 straddles two. If you need a tile
size, sweep for it; don't derive it.

### "Avoid powers of two" is not the rule

`lda = 4352` is not a power of two and is still 3.1x off the best.
`lda = 4097` is odd, so the row stride is not a whole number of cache lines,
the set index drifts across the whole array, and it performs like the
well-padded cases.

The rule is **how many factors of two the row length contains when measured in
cache lines** — which is exactly what the `gcd` formula above counts.

### Huge pages make it worse

Backing the unpadded matrices with 2 MB pages instead of 4 KB pages takes the
naive transpose from ~184 ms to ~355 ms. With 4 KB pages the OS scatters the
virtual-to-physical mapping, which partially breaks up congruence further down
the hierarchy; a huge page makes the physical addresses exactly as congruent as
the virtual ones. Worth knowing before reaching for `MADV_HUGEPAGE` as a
general-purpose speedup.

## Measurement notes

Two machine properties will corrupt these numbers if ignored, and both are
handled in the source:

1. **Heterogeneous cores.** cpu0–3 are Zen 5 at 5.13 GHz, cpu4–11 are Zen 5c
   at 3.17 GHz — a 1.6x spread depending on where the scheduler puts you. The
   benchmark pins to cpu0.

2. **The clock only boosts on high IPC.** `amd-pstate-epp` raises the clock in
   response to instructions-per-cycle, not to the core being busy. The
   transpose is memory-bound and low-IPC, so left alone it runs the entire way
   at ~3.59 GHz: **263 ms on all 8 repetitions cold, 184 ms on all 8 after a
   warm-up spin.** Min-of-N does not help — every repetition is equally slow,
   and the wrong answer looks perfectly stable. The benchmark spins on a
   high-IPC loop first and prints the clock at both ends of the run.

## Key takeaways

1. **Ask the reuse question first.** A transpose reads every element once, so
   tiling has no reuse to capture and can only improve spatial locality. That is
   what makes this kernel's answer come out the way it does — and why it does not
   transfer. [matmul_tiling.md](matmul_tiling.md) is the same transformation on a
   kernel that reads each element N times, and there tiling wins outright.

2. **In this kernel, tile size is the wrong variable.** The leading dimension
   governs the whole effect; sweeping tile sizes on a pathological `lda`
   optimises the symptom. Pad first — it is a one-line change, it beat the best
   tile size by 1.4x, and once it is done tiling here is a net loss.

3. **The rule is factors of two in the row length measured in cache lines**,
   not "avoid powers of two." `sets_reachable = 64 / gcd((lda/8) % 64, 64)`,
   and you want at least 16.

4. **The capacity formula ignores set conflicts entirely.** When a stride
   aliases, effective capacity is `associativity × line_size`, not `L1_size` —
   and even then the formula only brackets the answer. Sweep for the tile.

5. **Tiling is still real when you cannot fix the layout.** 5.5x at B=16 on a
   stride you are stuck with is worth having; in *this* kernel it is just the
   second thing to try, not the first.

6. **Tiling and conflict-avoidance are independent.** They fix different
   problems and compose — measured in [matmul_tiling.md](matmul_tiling.md),
   where padding helps the blocked version and does nothing for the unblocked
   one, while blocking wins ~3x either way.
