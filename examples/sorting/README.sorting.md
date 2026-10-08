# Sorting

A tour through why `std::sort` is built the way it is, and how far you can
push each of its pieces with SIMD.

## 1. How `std::sort` works: three techniques, and why small-sort matters

`std::sort` is introsort: quicksort with a depth-limited heapsort fallback
and an insertion-sort cutoff for small partitions. It combines three
techniques:

- **The Leaf** — small partitions go to insertion sort.
- **The Middle** — larger partitions run quicksort with a
  median-of-three pivot.V
- **The Safety Net** — recursion depth past `2·log₂n` switches to heapsort,
  bounding worst-case behavior.

The two standard libraries fill in that skeleton differently, so there are
two pages:

- [three_regimes.html](three_regimes.html) takes apart **GCC's libstdc++**,
  the textbook version: stop partitioning at ≤16 elements, finish them all
  with one final insertion-sort pass, and partition with a branchy Hoare loop.
- [libcxx_sort.html](libcxx_sort.html) covers what **clang's libc++** adds:
  insertion sort below 24 elements inside each partition, branch-free sorting
  networks for 3–5 elements, a median of medians of three above 128 elements,
  and — the one that matters — a *branch-free* bitset partition.

[sort_mechanisms.html](sort_mechanisms.html) covers the mechanics of each
underlying algorithm (insertion sort, quicksort, heapsort) individually.

The control is [quicksort.cpp](quicksort.cpp), a median-of-three Hoare
quicksort with no cutoff and no depth limit.
[timing_size_breakdown.md](timing_size_breakdown.md) instruments it
([quicksort_breakdown.cpp](quicksort_breakdown.cpp)) and shows that 14–51% of
its runtime is spent inside partitions of ≤16 elements, from n=32 to
n=1,048,576: raw quicksort recurses all the way down to partitions of size 1,
and each call pays a fixed cost (function frame, median-of-3, partition loop)
that dwarfs the comparison work at that size.

**libstdc++: the cutoff, and nothing else.** Built with GCC, `std::sort` is
2.0× faster than quicksort at n=16, where it is just an insertion sort, and
only 1.10–1.19× faster from n=128 up. Above the leaf it does the same
partitioning as quicksort at the same cost; its whole advantage is that one
final insertion pass handles the ≤16 leaves for about half of what recursing
into them costs.

**libc++: the partition.** Built with clang, `std::sort` is 2.0× faster than
quicksort at n=16 and 3.3× faster at n=1,048,576 — 3× faster than libstdc++.
Quicksort's Hoare loop branches on every comparison with the pivot and
mispredicts about half of them on random data; libc++'s bitset partition
records the comparisons in bitmasks and never branches on them.
[sort_compare_large.cpp](sort_compare_large.cpp) compiles libc++'s own
introsort from the header with that partition switched on and off, everything
else identical: switched off, libc++ runs within 1–8% of libstdc++; switched
on, it is 2.9× faster at n=1,048,576.

Two libc++ details matter for anyone measuring this. `std::sort` on `int`
with the default comparison is not compiled into your program: libc++
declares it an `extern template`, and the call goes to a copy precompiled into
the system's `libc++.1.dylib`. And a sort compiled from this SDK's headers
never takes the bitset partition, even with a comparator such as
`std::greater` that libc++ lists as branch-free, because the header's dispatch
checks the comparator without the `&` that list expects. The branch-free
partition reaches a program only through the precompiled copies.

**Benchmarks:** [std_sort.cpp](std_sort.cpp), [quicksort.cpp](quicksort.cpp),
[insertion_sort.cpp](insertion_sort.cpp),
[sort_compare_large.cpp](sort_compare_large.cpp),
[quicksort_breakdown.cpp](quicksort_breakdown.cpp)

**Measurement:** every program pins itself to a performance core, warms the
clock up and prints the clock it measured, and gives every sort a different
random input — sorting the same array over and over lets the branch predictor
learn it, and makes every branchy sort look several times faster than it is.
Times are the best of 7 trials; the tables in these files are the median of 5
runs on an Apple M5 at 4.3–4.5 GHz, clang `-O2` — and, for the libstdc++
numbers, Homebrew GCC 16 at `-O2`. `/usr/bin/g++` on a Mac is clang; the real
GCC is `g++-16`, and it needs the SDK path to find the system headers:
`g++-16 -O2 -std=c++17 -isysroot $(xcrun --show-sdk-path) std_sort.cpp -o std_sort`.

**Why `-O2`, and what `-O3` does.** Every build line here uses `-O2`. Moving
to `-O3` changes nothing measurable above n=16 — every size from 64 to
1,048,576, in `std_sort.cpp`, `quicksort.cpp` and `insertion_sort.cpp`, lands
within 3% of its `-O2` time.
At n=16 it is a different story, and not the one you would guess:

| n = 16 | `-O2` | `-O3` | |
|---|---:|---:|---:|
| std::sort, in `sort_compare_n16.cpp` | 58.2 ns | 82.3 ns | 1.41× slower |
| insertion sort, in `sort_compare_n16.cpp` | 62.2 ns | 84.9 ns | 1.36× slower |
| insertion sort, in `insertion_sort.cpp` | 71.6 ns | 71.7 ns | unchanged |
| std::sort, in `std_sort.cpp` | 63.5 ns | 84.0 ns | 1.32× slower |
| bitonic NEON / Highway, in `sort_compare_n16.cpp` | 8.5 / 10.6 ns | 8.6 / 10.6 ns | unchanged |

The same insertion sort slows by a third in one program and not at all in
another, so it is not about the sort's own code. The likeliest explanation — not yet pinned down —
is that at n=16 a sort takes a few dozen nanoseconds, and what `-O3` does to
the code *around* the call, how much it inlines into the benchmark loop and
how it lays that loop out, matters as much as the sort itself. The
lesson for measuring anything this small is to keep the flags fixed, and to
distrust a 1.4× difference that a flag change can produce or erase. (`-O3`
figures: median of 3 runs, same harness and machine.)

## 2. Bitonic sorting and optimizing the small-sort base case

If insertion sort is already a win over recursing, can a fixed sorting
network beat insertion sort at the leaf? [small_sorting.md](small_sorting.md)
benchmarks six n=16 implementations on Apple M5/NEON:

| Algorithm | Time | vs std::sort |
|---|---|---|
| bitonic NEON | 8.5 ns | 6.8× faster |
| bitonic Highway (portable) | 10.6 ns | 5.5× faster |
| bitonic scalar (80 CAS) | 23.7 ns | 2.5× faster |
| std::sort (introsort) | 58.2 ns | 1.00× (reference) |
| insertion sort | 62.2 ns | 1.07× slower |
| quicksort (median-of-3) | 120–140 ns | 2.1–2.4× slower |

Even the scalar bitonic network (80 branchless compare-and-swaps) is 2.5×
faster than `std::sort` — which at n=16 is an insertion sort — because on
random input the branchy sorts pay for mispredictions and the network never
does. Packing the same 10-step network into NEON registers (4 elements per
instruction) cuts it from 337 instructions to 145 and makes it the fastest
implementation by far. [bitonic_diagram.html](bitonic_diagram.html) builds the
network step by step, from a single comparator to a full traced run.

**Implementations:** [bitonic_sort.cpp](bitonic_sort.cpp) (scalar),
[bitonic_sort_simd.cpp](bitonic_sort_simd.cpp) (raw ARM NEON),
[bitonic_sort_highway.cpp](bitonic_sort_highway.cpp) (portable Google
Highway) — compared in [sort_compare_n16.cpp](sort_compare_n16.cpp).

## 3. vqs_highway: vectorizing the partition

Section 2 speeds up a fixed-size sort. [vqs_highway.md](vqs_highway.md) applies
the same "replace scalar control flow with SIMD" idea to the *partition* step
itself — the part of quicksort that dominates at large n. Following Bramas
([arXiv:1704.08579](https://arxiv.org/abs/1704.08579)), it replaces the
two-pointer scan with a **CompressStore scatter**: each N-element SIMD block
is classified against the pivot and packed into two scratch buffers with
`vpcompressd` (AVX-512) or a table-permute equivalent (AVX2, NEON). Its writes
are fully sequential, where an in-place partition's writes land wherever its
two pointers are.

To measure the partition and nothing else, everything else in `vqs_highway`
is libc++'s `std::sort`:

| | libc++ `std::sort` | `vqs_highway` |
|---|---|---|
| Base case | insertion sort below 24 | hands partitions below 24 to `std::sort` |
| Pivot | median of 3; median of medians of three above 128 | the same rule, same positions |
| Depth limit | heapsort past `2·log₂n` | the same |
| **Partition** | **branch-free bitset partition, in place** | **Highway `CompressStore`, two scratch buffers** |

Results on Apple M5, random `int32`, a different input for every sort, best
of 7 trials, median of 5 runs:

| n | vqs_highway | std::sort | speedup |
|---|---|---|---|
| 32 | 225 ns | 202 ns | 0.90× |
| 128 | 991 ns | 987 ns | 1.00× |
| 1 024 | 8.74 µs | 9.62 µs | 1.10× |
| 8 192 | 77.5 µs | 89.3 µs | 1.15× |
| 65 536 | 670 µs | 749 µs | 1.12× |
| 131 072 | 1.42 ms | 1.62 ms | 1.14× |
| 1 048 576 | 12.6 ms | 14.6 ms | 1.16× |

Since only the partition differs, the difference is the partition's.
CompressStore is behind at n=32, even from 64 to 256, and ahead from n=512 by
a steady 1.10–1.17× all the way to a million elements. The lead does not grow
with n, and every size here fits in the M5's 16 MB L2, so it is not a cache
effect; it is a contest between two branch-free partitions, since
`std::sort`'s bitset partition never mispredicts either.

[vqs_diagram.html](vqs_diagram.html) walks through one NEON partition block,
a full pass, the Highway inner loop, and the write-pattern comparison against
Hoare.

**Implementation:** [vqs_highway.cpp](vqs_highway.cpp)

Comparable-scale sorts for context: [newintrosort.cpp](newintrosort.cpp) /
[newintrosort_heapsortfallback.cpp](newintrosort_heapsortfallback.cpp)
(introsort variants with an explicit heapsort safety net) and
[sort_compare_large.cpp](sort_compare_large.cpp) (large-n comparison
harness).
