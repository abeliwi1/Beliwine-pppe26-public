# vqs_highway: Vectorized Quicksort

**File:** `vqs_highway.cpp`  
**Paper:** Bramas, "A Novel Hybrid Quicksort Algorithm Vectorized using AVX-512 on Intel Skylake," arxiv:1704.08579  
**Platform:** Apple M5, ARM NEON (4-lane int32); portable via Google Highway  
**Build:**
```
clang++ -O2 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib \
  vqs_highway.cpp -o vqs_highway -lhwy
```

[vqs_diagram.html](vqs_diagram.html) walks the CompressStore partition step by
step — one NEON block, one full pass, the Highway inner loop, the write-pattern
comparison against Hoare, and the results below — open it in a browser before
reading the rest of this file.

---

## One difference from std::sort

`vqs_highway` is `std::sort` with one part replaced: the partition. Everything
else follows libc++'s `std::sort`, so the comparison between the two measures
the partition and nothing else.

| | libc++ `std::sort` | `vqs_highway` |
|---|---|---|
| Base case | insertion sort below 24 (networks for 3, 4, 5) | hands partitions below 24 to `std::sort` — the same code |
| Pivot | median of 3; median of medians of three above 128 | the same rule, from the same positions |
| Depth limit | heapsort past `2·log₂n` | the same (`std::partial_sort`) |
| Recursion | recurse left, loop right | the same |
| **Partition** | **branch-free bitset partition, in place** | **Highway `CompressStore` into two scratch buffers** |

Not reproduced: libc++'s check for an already-partitioned range and its
separate partition for runs of keys equal to the pivot. Both matter on
presorted or duplicate-heavy input, not on the random data measured here.

---

## simd_partition (Algorithm 3)

The core of the paper. Replaces the two-pointer scan with a
**CompressStore scatter**:

```
pivot_v = broadcast(pivot)                      // N copies of pivot in a register

for each N-element block a[i..i+N):
    v       = LoadU(a + i)
    lt_mask = Lt(v, pivot_v)                    // lane-wise v[j] < pivot
    ge_mask = Ge(v, pivot_v)
    CompressStore(v, lt_mask, left_buf  + lc)   // pack selected lanes, advance lc
    CompressStore(v, ge_mask, right_buf + rc)   // pack selected lanes, advance rc

scalar tail for remaining < N elements

memcpy left_buf  → a[lo .. lo+lc-1]
memcpy right_buf → a[lo+lc .. hi-1]
return lc
```

`CompressStore(v, mask, d, ptr)` writes the lanes where `mask` is true
contiguously from `ptr` and returns the count. On AVX-512 this is a single
`vpcompressd` instruction. AVX2 and NEON have no compress instruction, so
Highway looks up a permutation for the mask, shuffles the selected lanes to
the front and stores the whole vector; the lanes past the count are scratch
that the next store overwrites.

**Memory traffic vs an in-place partition:**

| | In-place (Hoare; libc++'s bitset partition) | CompressStore (vqs) |
|---|---|---|
| Reads | n (two-pointer scan) | n |
| Writes | 2 per swap, random positions | n sequential (scratch) + n (memcpy back) |
| Aliasing | in-place, self-aliasing swaps | no aliasing |

An in-place partition's writes land wherever its two pointers are, which costs
cache-line evictions once the array outgrows the cache. CompressStore's writes
are fully sequential; the hardware prefetcher handles all six streams (read a,
write left_buf, write right_buf, read left_buf, write a, read right_buf + write
a for the second memcpy). Every size measured below fits in the M5's 16 MB L2,
so the results below do not exercise this difference.

---

## simd_qs_core (Algorithm 4)

The recursive driver, shaped like libc++'s `__introsort`:

```
loop:
    if hi - lo + 1 < 24:          std::sort(a + lo, a + hi + 1); return
    if depth == 0:                heapsort the range; return
    depth -= 1

    choose_pivot(a, lo, hi)       // libc++'s rule; the pivot ends up at a[lo]
    lc = simd_partition(a, lo+1, hi+1, pivot, left_buf, right_buf)
    // After: a[lo+1 .. lo+lc] < pivot, a[lo+lc+1 .. hi] ≥ pivot

    p = lo + lc
    swap(a[lo], a[p])             // the last element < pivot trades places
                                  // with the pivot, which lands at p
    recurse on [lo, p-1]
    lo = p + 1                    // loop on the right side
```

---

## Scratch Buffer Design

```cpp
void vqs_sort(vector<int>& v) {
    const int n = v.size();
    if (n < 24) { std::sort(v.begin(), v.end()); return; }
    vector<int> left_buf(n), right_buf(n);
    simd_qs_core(v.data(), 0, n - 1, 2 * (int)log2(n), left_buf.data(), right_buf.data());
}
```

Two buffers of size n are allocated once and passed by pointer through all
recursive calls. Each call to `simd_partition` resets its local `lc`/`rc`
counters from zero, so subarray partitions of any size stay within bounds.
Total extra allocation: 2n integers = 8n bytes (8 MB for n = 1M).

---

## Performance

Apple M5 performance core at 4.3–4.4 GHz after warm-up, clang `-O2`, random
`int32`. Every sort gets a different random input; each time is the best of 7
trials, and the table is the median of 5 runs.

| n | vqs_highway | std::sort | speedup | ns/(n log₂ n) |
|---|---|---|---|---|
| 32 | 225 ns | 202 ns | 0.90× | 1.41 |
| 128 | 991 ns | 987 ns | 1.00× | 1.11 |
| 512 | 4.25 µs | 4.66 µs | **1.10×** | 0.92 |
| 1 024 | 8.74 µs | 9.62 µs | **1.10×** | 0.85 |
| 8 192 | 77.5 µs | 89.3 µs | **1.15×** | 0.73 |
| 65 536 | 670 µs | 749 µs | **1.12×** | 0.64 |
| 131 072 | 1.42 ms | 1.62 ms | **1.14×** | 0.64 |
| 524 288 | 6.12 ms | 7.07 ms | **1.16×** | 0.61 |
| 1 048 576 | 12.6 ms | 14.6 ms | **1.16×** | 0.60 |

Since the two sorts differ only in the partition, the whole difference is the
partition's. CompressStore is behind at n = 32, even at 64 to 256, and ahead
from n = 512 by a steady 1.10–1.17× all the way to a million elements.

`ns/(n log₂ n)` falls with n for both sorts — vqs from 1.41 at n = 32 to 0.60
at n = 1 048 576 — so the ratio between them holds. The lead does not grow with
n, and every size here fits in the M5's 16 MB L2 (at n = 1 048 576, 4 MB of
data and 8 MB of scratch), so it is not a cache effect. Both partitions are
branch-free: `std::sort`'s is libc++'s bitset partition (see
[timing_size_breakdown.md](timing_size_breakdown.md)), so vqs is competing
against a partition that already never mispredicts.
