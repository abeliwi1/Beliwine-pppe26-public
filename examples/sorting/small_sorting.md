# Small-Array Sorting: Algorithms and SIMD at n=16

## Overview

Sorting small, fixed-size arrays is a surprisingly deep problem.  At large n
(thousands to millions of elements), O(n log n) algorithms dominate and the
differences between them are modest.  At n=16 the landscape inverts: recursion
overhead, branch mispredictions, and loop setup costs often exceed the actual
comparison work, and the choice of algorithm changes the result by 16×.

This document covers six implementations benchmarked at n=16 on an Apple M5
performance core, measured at 4.3–4.5 GHz after warm-up, clang `-O2`.  Every
sort gets a different random `int32` array, because sorting the same array
over and over lets the branch predictor learn it.  Each time is the best of 7
trials, and the table is the median of 5 runs.

## Results

| Algorithm | Time | ns/elem | vs std::sort |
|---|---|---|---|
| **bitonic NEON** | **8.5 ns** | **0.53** | **6.8× faster** |
| bitonic Highway (portable) | 10.6 ns | 0.66 | 5.5× faster |
| bitonic scalar (80 CAS) | 23.7 ns | 1.48 | 2.5× faster |
| std::sort (introsort) | 58.2 ns | 3.64 | 1.00× (reference) |
| insertion sort | 62.2 ns | 3.89 | 1.07× slower |
| quicksort (median-of-3) | 120–140 ns | 7.5–8.8 | 2.1–2.4× **slower** |

The two SIMD rows are the same to within 0.1 ns on every run.  The branchy
rows move more from run to run (std::sort 54–67 ns, quicksort 120–154 ns,
scalar bitonic 19–29 ns), but the order never changes.

## Algorithm Analysis

### std::sort — insertion sort in disguise (58 ns)

`std::sort` in libc++ is an introsort: quicksort with a heapsort fallback and
an insertion-sort cutoff for small partitions.  libc++'s cutoff is 24
elements, so at n=16 it never partitions at all — it goes straight to
insertion sort.  That is why it and the hand-written insertion sort below land
within 7% of each other.

### Quicksort — median-of-3, no cutoff (120–140 ns)

Classic recursive quicksort with median-of-three pivot selection but no
small-partition optimisation.  At n=16 the recursion tree is about 4 levels
deep, each level paying a function call, a median-of-three selection, and a
partition loop whose branches follow the data.  The algorithm does correct
O(n log n) work, but the constant factor is large relative to n=16.  This
implementation intentionally omits the small-partition cutoff to isolate
recursion overhead, and it is 2.1–2.4× slower than `std::sort` for it.

### Insertion sort (62 ns)

Insertion sort is iterative, has near-zero setup cost, and accesses memory
sequentially.  At n=16 its quadratic cost is at most 120 compare-and-shift
steps, each a simple shift with no recursive call and no pivot selection.  Its
weakness is the inner loop's exit test: on random data it is unpredictable,
and the predictor misses it about once per element.  `std::sort` is at least
as fast at every size measured, since below 24 elements it *is* an insertion
sort.

### Bitonic scalar — 80 CAS, branchless (23.7 ns, 2.5× faster than std::sort)

[bitonic_diagram.html](bitonic_diagram.html) builds the network up step by
step, from a single comparator to a full traced run — open it in a browser
before reading the rest of this section.

The bitonic sorting network executes a predetermined sequence of 80
compare-and-swap (CAS) operations with no branches, no recursion, and no
data-dependent control flow.  clang compiles each CAS to a compare and two
conditional selects, 337 instructions in all (160 `cmp` + 160 `csel` + loads
and stores), and not one branch.  It is 2.5× faster than `std::sort`, because
on random input the branchy sorts pay for mispredictions and the network never
does.

### Bitonic NEON — fastest overall (8.5 ns, 6.8× faster than std::sort)

The same 10-step bitonic network ported to ARM NEON processes all 16 elements
packed into four `int32x4_t` registers.  Each comparator step operates on 4
elements simultaneously with `vminq`/`vmaxq`/`vrev64q`/`vextq`.  clang
compiles the whole network to 145 instructions — 34 `smin`, 34 `smax`, 28
`rev64` and 44 lane shuffles and moves — against 337 for the scalar network.
The constant-mask `vbslq_s32` blends in the source become lane moves; there is
no `bsl` in the output.  2.3× fewer instructions, 2.8× faster than the scalar
network.  This implementation is the ARM-specific version of Algorithm 1 from
[arXiv:1704.08579](https://arxiv.org/abs/1704.08579).

### Bitonic Highway — portable SIMD (10.6 ns, 5.5× faster than std::sort)

Google Highway rewrites the same network using portable SIMD intrinsics
(`Reverse2`, `CombineShiftRightBytes`, `OddEven`, `LowerHalf`/`UpperHalf`/
`Combine`) that compile to NEON on ARM and to SSE4/AVX2/AVX-512 on x86 from
a single source file.  On this machine it selects NEON and runs in 10.6 ns —
25% slower than the raw NEON version.  It compiles to 197 instructions: the
same 140-instruction core of `smin`/`smax`/`rev64`/shuffles, plus 64 extra
register moves and `dup`s where the "lower half from one result, upper half
from another" blends are assembled from `LowerHalf`/`UpperHalf`/`Combine`.
The portability is the payoff.

## Key Takeaways

**Recursion overhead dominates at n=16.**  The hand-written quicksort is the
slowest algorithm despite having O(n log n) complexity: 2.1–2.4× slower than
`std::sort`, which does not recurse at this size.

**Branch-free wins at n=16 on random data, even in scalar code.**  The scalar
network does 80 compare-and-swaps where insertion sort does about 60 shifts,
and it is still 2.5× faster than `std::sort`, because none of its instructions
is a branch the predictor can miss.

**SIMD is the right tool for fixed small-n sorts.**  When n is a power of two
and small enough to fit in a handful of registers, a sorting network is the
ideal structure: it maps directly onto SIMD min/max/permute pipelines and
eliminates all control flow.  The NEON version runs 4 comparators per
instruction and is 6.8× faster than `std::sort`.

**std::sort's cutoff is the right engineering trade-off.**  Library introsort
wins the scalar comparison-sort category not through a clever algorithm but
through an engineering choice: fall back to insertion sort below a threshold
(24 elements in libc++, 16 in libstdc++).  That is why `std::sort` is 2.1–2.4×
faster than hand-rolled quicksort at n=16.

**Portable SIMD incurs a measurable but acceptable cost.**  Highway is 25%
slower than raw NEON at n=16 (10.6 ns vs 8.5 ns), but it is correct on every
Highway-supported target.  For an embedded sort kernel used as a building
block in a larger hybrid algorithm (e.g., as the base case of a vectorised
merge sort), this trade-off is usually worthwhile.

## Files

| File | Description |
|---|---|
| `std_sort.cpp` | `std::sort` benchmark, n=16 to 2²⁰ |
| `quicksort.cpp` | Recursive median-of-3 quicksort, n=16 to 2²⁰ |
| `insertion_sort.cpp` | Insertion sort with ns/n² column, n=16 to 2¹⁶ |
| `bitonic_sort.cpp` | Scalar bitonic network, n=16 only |
| `bitonic_sort_simd.cpp` | ARM NEON bitonic network, n=16 only |
| `bitonic_sort_highway.cpp` | Google Highway bitonic network, n=16 only |
| `sort_compare_n16.cpp` | Combined benchmark producing this table |
