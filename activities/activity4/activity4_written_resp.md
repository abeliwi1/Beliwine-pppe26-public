# Activity 4 — Written Responses

Platform: macOS on Apple silicon (`activity4.M5.md`).
Answers (a)–(g) and machine details for the written-work submission.

## Machine details

Recorded with the results. Other CPU-heavy programs were closed and the
machine was plugged in while measuring.

| Item | Value |
|---|---|
| Mac model | MacBook Pro, Apple M4, 16 GB |
| macOS version | 27.0.1 |
| OpenCilk version | OpenCilk 3.0.0 (clang/LLVM 19.1.7, commit `cd8ccfc1fd8293654dde7bf26657f08f89c7e54b`), `~/opencilk/bin/clang++` |

Workers used for the timed runs: 4.

## Part 2 — Measure and plot

### (a) Speedup across versions

Which input gives nearly the same speedup for all six versions? On which
input do the versions differ the most? Give the best and the worst version
on that input, with their speedups.

`trig` gives nearly the same speedup for all six versions. The median of
the three runs is between 3.54x (`cilk_for`) and 3.60x (`cilk_spawn`). Every
panel costs about the same, so the schedule barely changes the result.

`exp` is where the versions differ the most. The best are `cilk_spawn` and
`cilk_for_g1`, both with a median of 3.88x. The worst is `omp_static`, at
1.15x on every run. That gap is wider than on `poly`, where the medians run
from 2.26x (`omp_static`) to 3.58x (`omp_dynamic`). On `exp` the work piles
up near `x = 1`, so a static split leaves one thread with almost all of it.



## Part 3 — Static vs. dynamic scheduling

### (b) Static schedule and work per quarter

With a static schedule and 4 threads, which panels does each thread
integrate? From the work in each quarter, what is the best speedup a static
schedule could reach on each input? How close does the measured `omp_static`
speedup come to it?

`schedule(static)` gives each thread one contiguous block, assigned before
the loop starts. With 1024 panels and 4 threads that is 256 panels each,
and those blocks are the four quarters `skew` times:

| Thread | Panels | Interval |
|---|---|---|
| 0 | 0–255 | [0, 0.25] |
| 1 | 256–511 | [0.25, 0.50] |
| 2 | 512–767 | [0.50, 0.75] |
| 3 | 768–1023 | [0.75, 1] |

The run finishes when the slowest thread finishes, so the best static
speedup is `1 / (largest quarter's share of the work)`.

| Input | Work by quarter | Largest share | Best static speedup | Measured `omp_static` (median) |
|---|---|---|---|---|
| `trig` | 24.62%, 26.33%, 24.73%, 24.32% | 26.33% | 3.80x | 3.55x |
| `poly` | 30.00%, 15.53%, 14.63%, 39.84% | 39.84% | 2.51x | 2.26x |
| `exp` | 0.22%, 1.54%, 11.67%, 86.57% | 86.57% | 1.16x | 1.15x |

`exp` is on that ceiling: thread 3 holds 86.57% of the work, and the
measured speedup is 1.15x. `trig` and `poly` land about 0.25x under it
(3.55 against 3.80, 2.26 against 2.51). The bound assumes equal cores and
no overhead. This machine only requests performance cores, and the parallel
loop adds a little cost, so an even split cannot quite hit the quarter bound.
On `exp` that extra cost is hidden inside the long last quarter.

### (c) When dynamic scheduling helps

What properties of the data make dynamic scheduling improve performance over
static scheduling? Explain the expected differences among the three inputs
and compare with the measurements. Discuss any departures from the expected
trend.

Dynamic scheduling helps when panels that sit next to each other cost very
different amounts. `schedule(static)` hands out equal counts of panels, so
it is an even split of the work only when those panels take similar time.
`schedule(dynamic, 1)` hands a free thread one panel at a time, so a thread
that finished a cheap panel comes back and takes another, including one from
a heavy part of `[0, 1]`.

The `skew` times say how large that effect should be:

- `trig` is almost flat. Quarters are 24–26%, and a panel takes 148–722 µs.
  Dynamic should match static.
- `poly` is heavy at both ends (30% and 40%) and light in the middle (about
  15% each). The two middle threads finish early under static, so dynamic
  should pick up most of that idle time.
- `exp` is the extreme case: 86.57% of the work is in the last quarter and
  0.22% is in the first. Static leaves three threads idle. Dynamic should
  gain the most.

The medians follow that order. `omp_dynamic` minus `omp_static` is +0.01x
on `trig` (3.56x against 3.55x), +1.32x on `poly` (3.58x against 2.26x),
and +2.71x on `exp` (3.86x against 1.15x).

One departure: on even work the extra scheduling can make dynamic a bit
slower, and here it does not. A panel is tens to hundreds of microseconds,
so the cost of handing out the next one does not show up. The other is that
dynamic still stops short of 4x, at 3.58x on `poly` and 3.86x on `exp`.
One panel is still serial. On `poly` the slowest panel took 13.3 ms and the
costliest 1% of panels hold 13% of the work; on `exp` the slowest panel is
942 µs and the costliest 1% hold 7%. Whoever draws those panels keeps them
for the whole panel, so the last bit of imbalance remains.


## Part 4 — Grain size

### (d) OpenMP default dynamic chunk size (1 point)

What is OpenMP's default chunk size for `schedule(dynamic)` when none is
given? Does it depend on the number of iterations or threads?

It is 1. The OpenMP specification says that when `schedule(dynamic)` is
given with no `chunk_size`, the chunk size defaults to 1. That constant
does not depend on the number of iterations or the number of threads. Each
request hands the free thread one iteration, until none remain.

### (e) OpenCilk default grain size (2 points)

What is the default grain-size heuristic in this OpenCilk version? Cite the
compiler/runtime version and a documentation or implementation source. Give
the formula (and any rounding) and its value for this loop (1024 panels) on
4 workers. How many grains does the loop become, and what fraction of
`[0, 1]` does each one cover?

Compiler/runtime: OpenCilk 3.0.0 (clang/LLVM 19.1.7, commit `cd8ccfc`).

Source: `__cilkrts_cilk_for_grainsize_32` in the OpenCilk runtime. The
copy in this install is the bitcode `libopencilk-abi_osx-arm64.bc`, compiled
from `cilk2c_inlined.c` in the Cheetah runtime. It loads the worker count
`P`, divides the trip count by `8P` with unsigned integer division, then
clamps the result.

Formula, with `floor` from that unsigned division:

```
grain = min(2048, max(1, floor(n / (8 * P))))
```

For this loop, `n = 1024` and `P = 4`:

```
floor(1024 / (8 * 4)) = floor(1024 / 32) = 32
```

The clamp leaves it at 32. The loop becomes `1024 / 32 = 32` grains, and
each grain covers `32 / 1024 = 1/32` of `[0, 1]`.

### (f) `cilk_for` vs. `cilk_for_g1` (3 points)

Compare `cilk_for` and `cilk_for_g1` on the plot. Does a grain size of 1
improve the uneven inputs on this machine? Explain the result, including any
differences from the expected trend. Use the `skew` output to estimate how
much of the work the costliest grain holds under the default grain size, and
what that does to the other workers.

A grain is never split, so the worker that takes a grain finishes every
panel in it. Grain size 1 should help where those panels have very different
costs, and it should do little on `trig`, where they do not. The medians:

| Input | `cilk_for` (grain 32) | `cilk_for_g1` (grain 1) |
|---|---|---|
| `trig` | 3.54x | 3.56x |
| `poly` | 2.61x | 3.30x |
| `exp` | 3.09x | 3.88x |

Grain size 1 does improve the uneven inputs, and it leaves `trig` where it
was.

On `trig` the quarters are 24–26%, so a grain of 32 panels is about 1/32 of
the work. No worker is stuck on a heavy grain, and making the grains smaller
does not buy anything. It also does not cost anything visible: a panel is
already 148–722 µs, so the extra spawns are cheap next to the panel.

On `exp` the quarters are 0.22%, 1.54%, 11.67%, 86.57%. Each quarter is
about 7.4 times the one to its left, and 7.4 is `e^2`. A quarter is a step
of 0.25 in `x`, so the work per panel follows the chirp's frequency,
`e^{8x}`. A default grain is a step of `1/32`, and the last one covers
`[31/32, 1]`. Its share of the work is

```
(e^8 - e^{7.75}) / (e^8 - 1) ≈ 22%
```

The grain just before it, `[15/16, 31/32]`, holds about 17%. One worker
that draws the last grain has 22% of the serial work, and the other workers
cannot take any of those 32 panels. That alone would still allow a speedup
near 4. The measured 3.09x says the slowest worker held about 32%, so it
kept the last grain and part of the next heavy one. The other workers
finish the light left side and wait. Grain size 1 breaks that up. The
slowest single panel is 942 µs, under 1% of the 132 ms serial time, and the
costliest 1% of panels hold only 7% together. The median rises to 3.88x.

On `poly` the heavy quarters are the two ends, 30.00% and 39.84%. The
costliest 1% of panels, about 10 panels, hold 13% of the work, and they sit
at an end, inside one grain of 32. The measured 2.61x means the slowest
worker held about 38% of the work, almost the entire 39.84% end: eight
grains of 32 were not enough pieces for the other workers to steal that end
apart. Grain size 1 raises the median to 3.30x. It does not reach `exp`'s
3.88x, because one panel at the end took 13.3 ms, about 5% of the serial
time, and several of those stay with whoever draws them.

The one departure from the expected trend is `exp`'s default-grain result.
A last grain of 22% should have left room for a speedup near 4, and 3.09x
is well short of that. The grains next to it are heavy too, and one worker
keeps more than the single costliest grain.

### (g) What the two defaults assume (1 point)

(d) and (e) are very different defaults for the same job. What does each one
assume about the iterations of a loop? Which assumption fits this loop?

OpenMP's default of 1 assumes the iterations may cost very different
amounts, so the unit of scheduling should be one iteration. It does not try
to guess an equal split from `n` and the thread count.

OpenCilk's `floor(n / (8P))`, clamped to `[1, 2048]`, assumes the iterations
cost about the same. A grain of that size is large enough to hide the spawn,
and eight grains per worker leaves extra pieces for another worker to steal.
That is a safe assumption only when one grain is interchangeable with another.

This loop's iterations are the panels. On `trig` they are interchangeable,
so the Cilk assumption fits. On `poly` and `exp` they are not: a grain of 32
at a heavy end is a large fraction of the work. The unit-chunk assumption
fits the loop as it is run here. Grain size 1 matches the default on `trig`
and beats it on the other two.


