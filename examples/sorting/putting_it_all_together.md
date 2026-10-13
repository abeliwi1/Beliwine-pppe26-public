# Putting It All Together: Sorting with AVX-512 on Zen 5

**File:** [avx512_sort.cpp](avx512_sort.cpp)
**Machine:** AMD Ryzen AI 9 HX 370 (Zen 5, AVX-512), one of its four full Zen 5 cores
**Build:**
```
clang++ -O2 -std=c++17 -stdlib=libc++ -I. avx512_sort.cpp -o avx512_sort \
  -lhwy -lhwy_contrib
./avx512_sort          # all sections, about 4 minutes
./avx512_sort 67       # only sections 6 and 7
```

---

## The question

This page puts together the two ideas from this lecture: a SIMD partition in
the middle of the sort ([vqs_highway.md](vqs_highway.md)), and a sorting
network at the leaves ([small_sorting.md](small_sorting.md)). It asks how
much each one is worth, how much more you get from wider vector registers,
and what separates the result from a production SIMD sort.

Three things make the comparison controlled:

- **One partition source, three widths.** Highway compiles the same partition
  code for SSE4 (4 lanes), AVX2 (8 lanes) and AVX-512 (16
  lanes) in one program, and all of them run on the same core. Lane count is
  the only thing that changes.
- **A branch-free baseline.** "std::sort" below is libc++'s introsort
  with its branch-free bitset partition. Ubuntu's precompiled libc++
  `std::sort<int>` does not use that partition (it runs at libstdc++'s
  speed), so the program calls libc++'s `__introsort` from the header with the
  bitset flag on, as [sort_compare_large.cpp](sort_compare_large.cpp) does.
- **The same harness.** Pinned to cpu0, a different random input for every
  sort, best of 7 trials. Every table is the median of 5 runs.

**About the clock.** This laptop would not boost: every run measured 2.01 GHz,
the chip's base clock, even in its performance power profile. The times below
are at that clock; to compare them with another machine, convert to cycles.
The ratios between them do not depend on it.

---

## 1. The leaf: a bitonic network in zmm registers

[bitonic_diagram.html](bitonic_diagram.html) explains the network. A zmm
register holds 16 `int32`, so a 16-element network runs inside one register: each of the 10 steps is a shuffle
(to bring each lane's partner alongside it), a `min`, a `max` and a blend.
`bitonic_leaf` extends this to 32, 64, 128 and 256 elements in 2, 4, 8 and 16
registers. It pads the last register with `INT_MAX`, which sorts to the end
and is never stored.

| n | std::sort | bitonic, AVX-512 | speedup |
|---:|---:|---:|---:|
| 16 | 266 ns | 15.0 ns | 17.7× |
| 32 | 663 ns | 39.3 ns | 16.9× |
| 64 | 1.55 µs | 91.2 ns | 16.9× |
| 128 | 3.41 µs | 225 ns | 15.1× |

At n = 16 that is 30 cycles against 525. Below 24 elements `std::sort` is an
insertion sort, and on random input it mispredicts the exit from its inner
loop about once per element. The network has no data-dependent branches at
all, so it pays nothing for mispredictions.

**A compiler lesson.** The first version of the 128-element leaf took 622 ns,
not 225 ns. Its loops over the 8 registers were plain `for` loops, and clang
did not unroll them at K = 8. Without unrolling, the register index is a
runtime variable, so the `__m512i v[8]` array has to live on the stack: the
disassembly showed 109 stack accesses and backward branches. The fix,
`for_regs<K>`, expands the loop at compile time, so every register index and
every blend mask is a constant.

---

## 2. The partition at 4, 8 and 16 lanes

Here is the cost of one partition pass on its own, in cycles per element (the
time to copy in a fresh input is subtracted). The pivot is the median, so the
split is 50/50 and as hard to predict as possible:

| n | bitset (std::sort) | 4 lanes | 8 lanes | 16 lanes |
|---:|---:|---:|---:|---:|
| 1 024 | 2.25 | 1.04 | 0.59 | 0.58 |
| 65 536 | 2.32 | 1.04 | 0.65 | 0.65 |
| 1 048 576 | 2.43 | 1.25 | 0.90 | 0.78 |

Going from 4 lanes to 8 halves the cost. Going from 8 to 16 does almost
nothing. Next, the whole sort at each width, with everything except the
partition left as `std::sort`'s:

| n | std::sort | 4 lanes | 8 lanes | 16 lanes |
|---:|---:|---:|---:|---:|
| 1 024 | 32.4 µs | 1.21× | 1.24× | 1.16× |
| 65 536 | 2.62 ms | 1.30× | 1.41× | 1.33× |
| 1 048 576 | 47.7 ms | 1.35× | 1.51× | 1.43× |
| 4 194 304 | 205 ms | 1.30× | 1.44× | 1.37× |

The partition alone runs 2–4× faster than libc++'s bitset partition (2.4
cycles per element), but the whole sort gains at most 1.5×, and 16 lanes is
*worse* than 8. The sort has two problems, and the next
two sections take them in turn:

1. **The leaves dominate.** Below 24 elements the sort still runs insertion
   sort. Vectorizing the partition speeds up a part that was already not
   where most of the time goes.
2. **This partition doesn't scale past 8 lanes.** At 16 lanes it costs 9–10
   cycles per vector, twice the 8-lane loop's 5. AVX-512 has to use two
   compresses per vector (one for the < pivot lanes, one for the >= pivot
   lanes), where AVX2 uses one table permute that puts both groups in order.
   Mobile Zen 5 also runs 512-bit instructions on 256-bit-wide units.
   Desktop Zen 5 has full-width units. We have not measured one, so we can't
   say how much of the gap each cause explains.

(One more lane-width detail: AVX-512 has two ways to write a compressed
vector, `vpcompressd` straight to memory, or into a register followed by a
plain store. Highway has a separate Zen 4 target because Zen 4 runs the
memory form slowly. On Zen 5 the two measure the same, 0.62 against 0.58
cycles per element at n = 1024.)

---

## 3. Replacing the leaf

Here the 16-lane partition stays the same and the leaf changes, from
insertion sort below 24 elements to the bitonic network up to a given size:

| n | insertion < 24 | bitonic ≤ 16 | ≤ 32 | ≤ 64 | ≤ 128 |
|---:|---:|---:|---:|---:|---:|
| 1 024 | 27.9 µs | 1.81× | 2.85× | 4.48× | 7.29× |
| 65 536 | 1.96 ms | 1.69× | 2.45× | 3.37× | 4.64× |
| 1 048 576 | 33.5 ms | 1.61× | 2.26× | 2.92× | 3.63× |

The leaf matters more than the partition. Larger leaves also mean fewer
partition levels: each doubling of the leaf removes one pass over the array.

---

## 4. Closing the gap to VQSort

Highway ships its own vectorized quicksort, `hwy::VQSort`
([vqsort-inl.h](https://github.com/google/highway/blob/master/hwy/contrib/sort/vqsort-inl.h)).
It uses the same two ideas, a SIMD partition and sorting-network leaves, and
it is still 2.1–2.2× faster than our sort with leaves of up to 64. To find
out why, this ladder adds VQSort's design choices to our 16-lane sort one at
a time. Each **step** column is the speedup over the row above:

| | n = 1 024 | step | n = 65 536 | step | n = 1 048 576 | step |
|---|---:|---:|---:|---:|---:|---:|
| ours: buffered partition, leaves ≤ 64 | 6.07 µs | | 582 µs | | 11.5 ms | |
| + in-place partition | 7.06 µs | 0.86× | 754 µs | 0.77× | 15.0 ms | 0.77× |
| + unrolled 4 vectors deep | 6.02 µs | 1.17× | 545 µs | 1.38× | 10.2 ms | 1.47× |
| + leaves up to 256 keys | 3.37 µs | 1.79× | 349 µs | 1.56× | 7.24 ms | 1.41× |
| + column-major network | 3.06 µs | 1.10× | 331 µs | 1.05× | 6.92 ms | 1.05× |
| + sampled pivot | 3.55 µs | 0.86× | 331 µs | 1.00× | 6.93 ms | 1.00× |
| **hwy::VQSort** | 2.74 µs | 1.30× | 271 µs | 1.22× | 5.39 ms | 1.29× |

**In-place partition.** Our partition writes into two scratch buffers and
`memcpy`s them back, so every element is read and written one extra time at
every level of the recursion. VQSort's partition loads a few vectors from
each end of the array into registers, which frees a gap at each end. It then
keeps loading from whichever end has the smaller gap, and writes the < pivot
lanes into the left gap and the >= pivot lanes into the right gap. There is
no buffer and no copy back.

On its own, this made the sort *slower*. The buffered loop loads from
`a + i`, an address known in advance, so the core can start many loads
early. In the in-place loop, which end to load from next depends on the
write positions, and those depend on the previous vector's compare results.
Every load waits on the previous vector: 0.92 cycles per element, against
0.58 buffered.

**Unrolling.** Loading and partitioning 4 vectors per step means that
dependency is paid once per 64 elements instead of once per 16. The
partition drops to 0.43–0.49 cycles per element, the fastest of all, and at
n = 1M it is twice as fast as the buffered version (0.43 against 0.78),
because the array no longer fits in the 1 MB L2 cache and the copy back goes
to L3. This is why VQSort's source says that "unrolling is important".

**Leaves up to 256.** This is the largest single step: it removes two levels
of partitioning and replaces them with straight-line network code.

**Column-major layout.** In our layout, element *e* of the network is lane
*e* mod 16 of register *e* / 16. The steps that compare nearby elements, 26
of the 36 steps in a 256-element network, then compare lanes of the same
register, and each needs a shuffle, a min, a max and a blend. VQSort arranges
the data the other way around: element *e* is lane *e* / 16 of register *e*
mod 16. Now those 26 steps compare two whole registers with just a min and a
max, and only 10 steps need a shuffle. The cost is one 16×16 transpose at the
end. The input doesn't need one, because an unsorted input can be loaded in
any order. On a single 256-element leaf this is 1.17× faster.

**Sampled pivot.** VQSort's pivot is the median of 32 samples, each sample
the median of 3 keys. On random input it gains nothing: libc++'s ninther is
already close to the median. The point of sampling is robustness against
sorted or skewed inputs, which these tables do not test.

**What is left (1.22–1.30×).** It is mostly the networks. Measured on one
leaf, VQSort's base case is 1.35× faster than our column-major network at
160–256 elements and 1.53× faster at 128. Section 5 looks at why.

One thing VQSort does *not* do differently on AVX-512: it also partitions
with two compress-stores per vector. Only on SSE4 and AVX2, where compress
puts both groups in order, does it use one compress and two plain stores.
Our partition does the same; on SSE4 the alternative is a masked store
(`maskmovdqu`), and it was 10× slower.

---

## 5. Better networks than bitonic

Bitonic sort is the usual choice for SIMD because it is regular: every step
compares each element with the one a fixed distance *j* away, so a whole step
is one shuffle pattern applied to every register. It is not small. For 16
inputs, all three of these networks have depth 10 (10 rounds of comparisons
that can run in parallel), but they differ in size:

| 16-input network | compare-exchanges | depth |
|---|---:|---:|
| bitonic sort | 80 | 10 |
| Batcher's odd-even merge sort | 63 | 10 |
| Green's network (smallest known) | 60 | 10 |

Batcher's and Green's networks are irregular: the pairs they compare follow
no fixed pattern, so they can't be expressed as one shuffle per step inside a
register. In the column-major layout they don't need to be. The first phase
of the 256-key network sorts each column, lane *l* of all 16 registers, and
each compare-exchange there is a `min` and a `max` of two whole registers. Any
16-input network can do that job, regular or not, and it runs on all 16
columns at once. This is where VQSort uses Green's network (its `Sort16`).

`avx512_sort.cpp` swaps the column phase of our 256-key network for each of
them. The program checks every network by the 0-1 principle: a comparator
network sorts all inputs if it sorts all 2¹⁶ inputs of 0s and 1s. Batcher's
and Green's networks sort every column ascending, but the bitonic merge that
follows expects the odd columns descending, so 16 blends reverse them. The
rest of the network (the merge stages and the transpose) is unchanged.

| | bitonic | Batcher | Green | VQSort |
|---|---:|---:|---:|---:|
| one leaf, n = 160 | 351 ns | 1.07× | 1.07× | 1.15× |
| one leaf, n = 256 | 356 ns | 1.07× | 1.07× | 1.15× |
| whole sort, n = 1 024 | 3.56 µs | 1.03× | 1.03× | 1.28× |
| whole sort, n = 65 536 | 331 µs | 1.02× | 1.02× | 1.22× |
| whole sort, n = 1 048 576 | 6.94 ms | 1.02× | 1.01× | 1.29× |

(Speedups are over bitonic. The whole sort is section 6's 16-lane sort.)

Green's network is 1.07× faster on one leaf, and Batcher's is just as good.
The instruction counts of the compiled 256-key networks show why the gain is
small:

| 256-key network | instructions | `min`/`max` | shuffles |
|---|---:|---:|---:|
| bitonic columns | 1 450 | 736 | 296 |
| Green columns | 1 365 | 696 | 296 |

Green's network saves 20 compare-exchanges per column (40 `min`/`max` over 16
registers), plus the per-lane blends that bitonic needs in its last column
stage: 85 instructions, 6% of the network. The time drops by the same 6–7%.
The column phase was never where the cost is: 26 of the network's 36 steps
are in the merge, and all 296 shuffles are in the merge stages and the
final transpose. A better column network can't touch either.

In the whole sort the gain shrinks to 1–3%, because only leaves of 129–256
keys use this network. Leaves of 128 or fewer still go to the row-major
networks, and that is the size where VQSort's lead is largest (1.53×).

So most of VQSort's remaining advantage is in its merge networks and its
shapes, not in its column sort:

- **The merge.** VQSort merges sorted columns with Mark Blacher's hybrid
  bitonic/odd-even merging networks, written out vector by vector by a
  generator. They reverse some registers and compare whole registers, and
  only then finish inside each register. The result comes out in memory
  order, so there is no transpose. (We have not measured how many shuffles
  they need.)
- **The shape.** VQSort keeps 16 rows at every size and narrows the row
  instead: 16 rows of 4 keys (128-bit vectors) for leaves up to 64, and 16
  rows of 8 (256-bit) up to 128. The cheap whole-register comparisons then
  dominate at every leaf size, not only at 256.

The general lesson: on SIMD hardware, comparator count is not the measure of
a network. What matters is how many comparisons are between whole
registers, and how many shuffles the rest need.

---

## 6. Width, with everything in place

Here is every VQSort choice at each width (in-place, unrolled ×4, leaves up
to 256 keys, column-major, sampled pivot), against `std::sort`:

| n | std::sort | 4 lanes | 8 lanes | 16 lanes | hwy::VQSort |
|---:|---:|---:|---:|---:|---:|
| 1 024 | 32.5 µs | 8.03× | 9.54× | 9.12× | 11.7× |
| 8 192 | 292 µs | 6.51× | 8.54× | 8.27× | 10.4× |
| 65 536 | 2.62 ms | 4.62× | 7.16× | 7.89× | 9.65× |
| 131 072 | 5.41 ms | 4.39× | 6.69× | 7.28× | 9.47× |
| 1 048 576 | 47.7 ms | 3.92× | 6.28× | 6.89× | 8.89× |
| 4 194 304 | 204 ms | 3.62× | 5.74× | 6.29× | 7.82× |

Now the widths separate. With insertion-sort leaves and a buffered partition
(section 2), 16 lanes lost to 8. Here 16 lanes beat 8 by 10% at n ≥ 64K and
4 lanes by 1.7–1.8×. At n = 1024 and 8192 the 8-lane sort still wins
slightly: the partitions there are short, and setting up a 16-lane partition
costs more.

---

## What this shows

- **Vectorizing the partition alone is a modest win**: 1.2–1.5× at any
  width, against a partition that was already branch-free. Vectorize the
  leaves as well and restructure the partition, and even 4 lanes give 3.6–8×.
- **Width pays only once nothing else hides it.** Changing the partition
  alone, 16 lanes was slower than 8. With the rest of the sort vectorized,
  16 lanes is the fastest at large n, and 4 lanes is 1.6–1.8× behind.
- **Layout matters as much as width.** In-place but not unrolled was a loss.
  Leaves of 256 instead of 64 gained more than any change of width. Changing
  the network's layout so that most comparisons are between whole registers
  gained another 5–10%.
- **Fewer comparators is not the same as faster.** Green's 60-comparator
  network saves 6% of the leaf's instructions and gains 1–3% overall. The
  cost is in the shuffles, and those are set by the layout and the merge.
- **A production library still wins by 1.2–1.3×**, mostly through better
  networks. This page's sort keeps libc++'s structure (pivot, depth limit,
  recursion) so that each step can be measured on its own.
