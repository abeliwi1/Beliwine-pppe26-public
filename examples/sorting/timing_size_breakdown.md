# Quicksort: Time Spent in Small Partitions (≤ 16 elements)

The first half of this file is clang with libc++, the default on a Mac; the
[last section](#with-gccs-libstdc) repeats the measurements with GCC and
libstdc++, and is the data behind [three_regimes.html](three_regimes.html).
[libcxx_sort.html](libcxx_sort.html) draws on both.

**Source:** [quicksort_breakdown.cpp](quicksort_breakdown.cpp), which instruments `quicksort.cpp`'s sort and prints both tables below.
**Setup:** `quicksort.cpp` (median-of-three, no small-partition cutoff), random `int` data, clang `-O2`, Apple M5 performance core at 4.3–4.5 GHz after warm-up. Every sort gets a different random input; each figure is the best of 7 trials, and the tables are the median of 5 runs.
**Instrumentation:** a guard flag times only the *outermost* call into the ≤16 regime, so each timed interval covers that call's entire subtree and nothing is counted twice. Each timed interval costs a pair of clock reads, 13 ns on this machine; that cost is measured and subtracted, since a ≤16 subtree takes only a few times as long.

## Results

| n | time | ns/elem | ns/(n log₂ n) | small (≤16) % |
|---|---|---|---|---|
| 32 | 392 ns | 12.2 | 2.45 | 47.9% |
| 64 | 954 ns | 14.9 | 2.48 | 40.2% |
| 128 | 2.12 µs | 16.6 | 2.37 | 35.7% |
| 256 | 4.93 µs | 19.3 | 2.41 | 31.6% |
| 512 | 11.1 µs | 21.6 | 2.40 | 28.1% |
| 1 024 | 23.7 µs | 23.1 | 2.31 | 25.7% |
| 2 048 | 53.0 µs | 25.9 | 2.35 | 23.5% |
| 4 096 | 117 µs | 28.5 | 2.38 | 21.8% |
| 8 192 | 247 µs | 30.2 | 2.32 | 20.6% |
| 16 384 | 537 µs | 32.8 | 2.34 | 19.1% |
| 32 768 | 1.14 ms | 34.9 | 2.32 | 17.8% |
| 65 536 | 2.40 ms | 36.7 | 2.29 | 17.0% |
| 131 072 | 5.05 ms | 38.5 | 2.27 | 16.2% |
| 262 144 | 10.9 ms | 41.8 | 2.32 | 15.1% |
| 524 288 | 22.9 ms | 43.7 | 2.30 | 14.6% |
| 1 048 576 | 48.0 ms | 45.8 | 2.29 | 13.9% |

(At n = 16 the root call is itself a ≤16 partition, so the share is 100% by
definition; the table starts at 32.)

## Observations

**`ns/(n log₂ n)` is flat across the whole range, 2.27–2.48.**
Quicksort scales as cleanly as O(n log n) gets, from 32 elements to a million.
There is no cache transition: even at n = 1 048 576 the array is 4 MB, well
inside the M5's 16 MB L2.

**The small-partition share falls steadily, from 48% at n = 32 to 14% at
n = 1 048 576.** Partitions of ≤16 elements are the bottom two or three levels
of the recursion tree. Their total work grows like n, while the levels above
them grow like n log n, so their share shrinks as the tree gets taller — by
3–8 percentage points per doubling below n = 512, and by about 1 point per
doubling above n = 8 192.

## Implication for optimization

An insertion-sort cutoff at ≤ 16 elements replaces exactly this share of the
work, so it can save at most:
- ~25–50% of runtime at small n (n ≤ 1024)
- ~14–20% of runtime at large n — a ceiling of about 1.16× at n = 1 048 576,
  even if the leaves became free

## Quicksort vs. std::sort over the same size range

`std::sort` is libc++'s introsort. For an `int` array with the default
comparison it does three things `quicksort.cpp` does not:

- **insertion sort below 24 elements**, inside each partition;
- **a median-of-medians-of-three pivot** above 128 elements;
- **a branch-free bitset partition**: it compares a block of elements against
  the pivot, records the outcomes in bitmasks, and swaps from the masks, so no
  branch depends on the data.

It also keeps the depth-limited heapsort fallback, which never fires on random
data.

Same machine and method; from `quicksort.cpp` and `std_sort.cpp`:

| n | quicksort ns/(n log₂ n) | std::sort ns/(n log₂ n) | std::sort speedup |
|---|---:|---:|---:|
| 16 | 1.94 | 0.99 | 1.95x |
| 32 | 2.41 | 1.17 | 2.06x |
| 64 | 2.49 | 1.25 | 2.00x |
| 128 | 2.49 | 1.17 | 2.12x |
| 256 | 2.43 | 1.09 | 2.23x |
| 512 | 2.43 | 1.00 | 2.42x |
| 1 024 | 2.37 | 0.96 | 2.48x |
| 2 048 | 2.37 | 0.93 | 2.55x |
| 4 096 | 2.32 | 0.87 | 2.66x |
| 8 192 | 2.34 | 0.83 | 2.81x |
| 16 384 | 2.35 | 0.80 | 2.95x |
| 32 768 | 2.32 | 0.76 | 3.04x |
| 65 536 | 2.30 | 0.73 | 3.13x |
| 131 072 | 2.28 | 0.72 | 3.16x |
| 262 144 | 2.33 | 0.71 | 3.29x |
| 524 288 | 2.30 | 0.70 | 3.30x |
| 1 048 576 | 2.29 | 0.69 | 3.34x |

Speedup is the ratio of the `ns/(n log₂ n)` columns, which cancels the shared
`n log₂ n` term and reduces to plain total-time ratio.

`std::sort` wins at every size, by 2.0x at n = 16 rising smoothly to 3.3x at
n = 1 048 576. **The cutoff cannot be the main reason at large n:** the table
above caps what a ≤16 cutoff can buy at about 1.16x for n = 1 048 576, and
`std::sort` is 3.3x faster.

**The partition is.** Measuring that needs a `std::sort` with the partition
switched off and everything else left alone, and two facts about libc++ on
macOS decide how to get one:

- **`std::sort` on `int` is not compiled into the program.** For built-in types
  with the default comparison, libc++ declares `std::__sort` an `extern
  template`, so the call goes to a copy precompiled into the system's
  `libc++.1.dylib`. That copy uses the bitset partition.
- **A sort compiled from this SDK's headers never uses it.** Even with a
  comparator libc++ lists as branch-free, such as `std::greater`, the header
  dispatch checks the comparator without the `&` that list expects, and picks
  the ordinary partition. Only the precompiled copies get the bitset partition.

So `sort_compare_large.cpp` calls libc++'s own `__introsort` directly, compiled
from the header, with the bitset-partition flag forced on and off. The cutoff,
the pivot choice, the 3/4/5-element networks and the heapsort fallback are the
same code in both, so their ratio is the partition alone. (These are libc++
internals; the call is specific to this libc++ version.)

| n | quicksort | std::sort | bitset on | bitset off | quicksort ÷ std::sort | off ÷ on (partition) | quicksort ÷ off (everything else) |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 024 | 24.5 µs | 9.93 µs | 9.95 µs | 19.0 µs | 2.47x | 1.91x | 1.29x |
| 2 048 | 54.9 µs | 19.9 µs | 20.8 µs | 42.3 µs | 2.76x | 2.03x | 1.30x |
| 4 096 | 119 µs | 42.0 µs | 43.3 µs | 93.0 µs | 2.83x | 2.15x | 1.28x |
| 8 192 | 254 µs | 87.0 µs | 88.2 µs | 203 µs | 2.92x | 2.30x | 1.25x |
| 16 384 | 547 µs | 175 µs | 182 µs | 438 µs | 3.13x | 2.41x | 1.25x |
| 32 768 | 1.17 ms | 380 µs | 371 µs | 960 µs | 3.08x | 2.59x | 1.22x |
| 65 536 | 2.42 ms | 707 µs | 740 µs | 2.00 ms | 3.42x | 2.70x | 1.21x |
| 131 072 | 5.15 ms | 1.62 ms | 1.59 ms | 4.39 ms | 3.18x | 2.76x | 1.17x |
| 262 144 | 11.1 ms | 3.33 ms | 3.32 ms | 9.30 ms | 3.33x | 2.80x | 1.19x |
| 524 288 | 23.2 ms | 7.02 ms | 6.96 ms | 20.0 ms | 3.30x | 2.87x | 1.16x |
| 1 048 576 | 48.8 ms | 14.5 ms | 14.3 ms | 41.2 ms | 3.37x | 2.88x | 1.18x |

With the flag on, the header build runs within a few percent of the library
copy (0.98–1.05x) — how the library was built adds nothing. The two factors
multiply to the total. The branch-free partition is worth 1.91x at n = 1 024,
growing to 2.88x at n = 1 048 576; everything else — the cutoff, the pivot
choice and the small networks — is worth 1.29x, shrinking to 1.18x, which
matches the ≤16 share measured above.

**Net effect:** the small-partition cutoff is worth what the breakdown says it
is — a quarter to a half of the runtime at small n, a sixth at large n. Most
of `std::sort`'s lead at large n comes from somewhere else: a partition step
whose comparisons against the pivot never mispredict. On random data a
branchy partition mispredicts about half of those comparisons, and that cost
recurs at every level of the recursion, which is why the partition's share of
the lead grows with n.

## With GCC's libstdc++

The same programs built with GCC 16.2 and its libstdc++
(`g++-16 -O2 -std=c++17 -isysroot $(xcrun --show-sdk-path)`), same machine
and method. libstdc++'s `std::sort` is the textbook introsort: a median-of-three
Hoare partition — the same kind `quicksort.cpp` uses — down to partitions of
≤16 elements, which it leaves alone, then one insertion-sort pass over the whole
array, plus the depth-limited heapsort fallback. Above the leaf it does the same
partitioning as `quicksort.cpp`, so it should cost what quicksort costs with its
≤16 leaves removed, plus the final pass.

`quicksort_breakdown.cpp` under GCC (pairs of clock reads cost 10 ns here):

| n | time | ns/elem | ns/(n log₂ n) | small (≤16) % |
|---|---|---|---|---|
| 32 | 391 ns | 12.2 | 2.44 | 50.8% |
| 64 | 929 ns | 14.5 | 2.42 | 42.8% |
| 128 | 2.16 µs | 16.9 | 2.41 | 37.7% |
| 256 | 4.87 µs | 19.0 | 2.38 | 33.3% |
| 512 | 10.8 µs | 21.1 | 2.34 | 30.2% |
| 1 024 | 23.6 µs | 23.0 | 2.30 | 27.4% |
| 2 048 | 51.3 µs | 25.0 | 2.28 | 25.4% |
| 4 096 | 112 µs | 27.3 | 2.28 | 23.1% |
| 8 192 | 239 µs | 29.2 | 2.24 | 22.2% |
| 16 384 | 506 µs | 30.9 | 2.21 | 21.2% |
| 32 768 | 1.09 ms | 33.3 | 2.22 | 19.4% |
| 65 536 | 2.35 ms | 35.9 | 2.24 | 18.2% |
| 131 072 | 4.91 ms | 37.5 | 2.20 | 17.7% |
| 262 144 | 10.5 ms | 40.1 | 2.23 | 16.8% |
| 524 288 | 22.3 ms | 42.5 | 2.24 | 16.1% |
| 1 048 576 | 46.7 ms | 44.5 | 2.23 | 15.6% |

And `quicksort.cpp` against `std::sort`, in ns/(n log₂ n). "Leaves removed"
is quicksort's cost times (1 − its ≤16 share); "leaf cost" is the part removed;
"final pass" is what `std::sort` costs beyond the leaves-removed line:

| n | quicksort | leaves removed | std::sort | std::sort speedup | leaf cost | final pass |
|---|---:|---:|---:|---:|---:|---:|
| 16 | 2.09 | — | 1.04 | 2.01x | — | — |
| 32 | 2.52 | 1.24 | 1.85 | 1.36x | 1.28 | 0.61 |
| 64 | 2.41 | 1.38 | 1.89 | 1.27x | 1.03 | 0.52 |
| 128 | 2.35 | 1.47 | 2.00 | 1.18x | 0.89 | 0.53 |
| 256 | 2.36 | 1.57 | 1.99 | 1.19x | 0.79 | 0.41 |
| 512 | 2.32 | 1.62 | 1.98 | 1.17x | 0.70 | 0.36 |
| 1 024 | 2.31 | 1.68 | 2.01 | 1.15x | 0.63 | 0.33 |
| 2 048 | 2.30 | 1.72 | 1.97 | 1.17x | 0.58 | 0.25 |
| 4 096 | 2.28 | 1.75 | 2.01 | 1.14x | 0.53 | 0.25 |
| 8 192 | 2.25 | 1.75 | 1.99 | 1.13x | 0.50 | 0.24 |
| 16 384 | 2.22 | 1.75 | 2.00 | 1.11x | 0.47 | 0.24 |
| 32 768 | 2.24 | 1.80 | 1.99 | 1.13x | 0.43 | 0.18 |
| 65 536 | 2.21 | 1.81 | 1.95 | 1.14x | 0.40 | 0.14 |
| 131 072 | 2.21 | 1.82 | 1.98 | 1.12x | 0.39 | 0.16 |
| 262 144 | 2.23 | 1.85 | 2.01 | 1.11x | 0.37 | 0.16 |
| 524 288 | 2.24 | 1.88 | 2.02 | 1.11x | 0.36 | 0.14 |
| 1 048 576 | 2.23 | 1.88 | 2.04 | 1.10x | 0.35 | 0.15 |

The prediction holds: `std::sort` sits just above the leaves-removed line at
every size, and the final pass costs 0.34–0.60 of what quicksort spends
recursing into the same leaves — about half. That half is libstdc++'s whole
advantage: 2.0x at n = 16, where the final pass is the entire sort, and
1.10–1.19x from n = 128 up.

Two cross-checks against the clang numbers above:

- **The compiler is not the difference.** `quicksort.cpp` built with GCC runs
  within 2–8% of the clang build at every size.
- **The partition is.** libc++'s introsort with the bitset partition switched
  off — everything else on — runs at 1.86–2.01 ns/(n log₂ n) from n = 1 024 to
  1 048 576, within 1–8% of libstdc++'s 1.95–2.04. With it on, libc++ is 0.69
  at n = 1 048 576: 2.96x faster than libstdc++.
