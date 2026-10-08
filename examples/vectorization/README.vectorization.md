
Standalone, one file, no Makefile needed. Both a hand-written path and
Highway build on either chip — AVX2 on x86, NEON on arm64 — picked at compile
time by `__x86_64__`/`__i386__` vs `__aarch64__`/`__arm__`:

```
# x86 (Zen 5, AVX2) — scalar, hand-written AVX2, and Highway
# (-mavx2 alone gives Highway SSSE3; see "One trap" at the end)
clang++ -O3 -mavx2 -mbmi -mbmi2 -mfma -mf16c -maes -mpclmul -std=c++17 \
        -stdlib=libc++ find_first.cpp -o find_first -lhwy

# arm64 (Apple M5) — scalar, hand-written NEON, and Highway
clang++ -O3 -std=c++17 -stdlib=libc++ find_first.cpp -o find_first -lhwy

./find_first
```

# Vectorization: a loop the compiler will not do for you

Auto-vectorization handles most simple loops well enough that hand-written
SIMD is usually not worth it. This is one of the cases where it is, and the
reason has nothing to do with being clever about the data.

Find the index of the first element equal to a target:

```c
long find_first(const int64_t* data, long n, int64_t target) {
    for (long i = 0; i < n; i++) {
        if (data[i] == target) return i;
    }
    return -1;
}
```


[find_first_diagram.html](find_first_diagram.html) draws the computation: the
scalar and vector scans side by side, what happens inside one vector step, and
why the vectorizer refuses this loop. Open it in a browser.


Ask the compiler why it did not vectorize this and it tells you:

```
$ clang++ -O3 -Rpass-analysis=loop-vectorize -c find_first.cpp
remark: loop not vectorized: could not determine number of loop iterations
```

A vectorizer plans the loop before running it: process `n/k` whole vectors,
then clean up the remainder. That plan needs the trip count up front. This
loop exits when it finds something, so the trip count depends on the data
and there is no plan to make. The compiler emits plain scalar code —
`cmp` / `je`, one element per iteration, no vector instructions at any
optimization level.



## Why this one counts

By hand it is straightforward. On x86, compare four at a time with AVX2,
stop at the first vector that contains a match, then find which lane:

```c
__m256i eq = _mm256_cmpeq_epi64(v, wanted);
int mask = _mm256_movemask_pd(_mm256_castsi256_pd(eq));   // one bit per lane
if (mask) return i + __builtin_ctz(mask);                 // lowest set bit wins
```

Done correctly it never loads outside the array — the loop condition is
`i + 4 <= n`, and the leftovers are handled scalar. It returns the same
index as the scalar version for every input, and `-1` in the same cases —
it assumes nothing about `n`, nothing about the values, and nothing about
how many matches exist.

NEON has no direct equivalent of `movemask`, so the arm64 version checks its
two lanes directly instead of building a bitmask:

```c
uint64x2_t eq = vceqq_s64(v, wanted);           // each lane: all-1s or 0
if (vgetq_lane_u64(eq, 0)) return i;
if (vgetq_lane_u64(eq, 1)) return i + 1;
```

Same contract, same guarantees — just two lanes instead of four, and two
branches instead of one `ctz`.

So this is not the usual SIMD trade, where you go faster by quietly
accepting a restriction the compiler was obliged to reject. The compiler is
not declining to vectorize this loop because it doubts you. It is declining
because its vectorizer is built around a trip count, and this loop does not
have one.

## How these were measured

The program pins itself to one core, spins on a high-IPC loop until the
governor raises the clock, and reports the minimum of seven timing passes
rather than the mean of one. Set `PIN_CPU` to move off a busy core:

```bash
PIN_CPU=2 ./find_first
```

All three matter at this scale. The times below are single-digit to
low-thousand nanoseconds per call, and this kernel is **compute-bound** — its
times scale with the core clock, unlike the memory-bound loops in
[../loop_optimizations/](../loop_optimizations/README.loops.md), which barely
move when the clock does. So a run on an efficiency core, or on a performance
core the governor has not yet boosted, reports much larger numbers, and the
scalar-versus-vector *ratios* shift too: the scalar path is clock-bound while
the short vector path is dominated by fixed overhead.

On Apple silicon, know what `PIN_CPU=6` actually buys you. The only QoS class
that confines a thread to the efficiency cluster also parks it at the lowest
clock and lets anything else preempt it, so an M5 efficiency run measures
**1.08 GHz and times roughly 4x the performance cluster's** — and it is
perturbed enough that both vector paths come out *slower* than scalar at most
lengths (0.62x at 256, 0.72x at 4096). Those are not usable numbers. Use the
efficiency cluster to see that the clock matters, not to measure how much.

**The numbers currently in this file were taken on a core held at 2.72 GHz**,
about half this part's 5.13 GHz boost. They are internally consistent — one
sitting, one clock — but the absolute nanoseconds are roughly double what a
boosted core gives. Each table below states the clock it was measured at, so a
replacement should state its own.

<!-- REPLACE: one sitting on a boosted Zen 5 core. Needs only this machine. -->
## Results: hand-written AVX2

8192 `int64` values (64 KiB, more than Zen 5's 48 KiB L1d, so partly served
from L2), Ryzen AI 9 HX 370, g++ 13.3,
`-O3 -mavx2` plus the flags Highway needs. Pinned to one core, clock warmed and
**measured at 2.72 GHz**, minimum of seven passes, median of three runs. All
three columns come from the same sitting, so they are comparable to each other.

| first match at | scalar | simd | highway | simd | highway |
|---|---:|---:|---:|---:|---:|
| 0 | 0.4 ns | 0.7 ns | 1.0 ns | 0.57x | 0.40x |
| 16 | 4.4 ns | 2.2 ns | 2.5 ns | 2.00x | 1.76x |
| 256 | 105.6 ns | 29.7 ns | 29.7 ns | **3.56x** | **3.56x** |
| 4096 | 1518.0 ns | 485.9 ns | 499.7 ns | 3.12x | 3.04x |
| 8191 | 3026.9 ns | 984.7 ns | 1207.2 ns | 3.07x | 2.51x |
| no match | 3050.1 ns | 974.0 ns | 1033.2 ns | 3.13x | 2.95x |

The scalar column reproduces to within 0.1% across runs. The only cell that
moves is Highway at 8191, which ranged 1185–1232 ns over three runs.

> **This was taken at 2.72 GHz, not the part's 5.13 GHz boost clock.** The
> machine has been sitting at roughly half its boost range since a reboot, and
> neither `platform_profile=performance` nor `energy_performance_preference=performance`
> lifts it. Because this kernel is compute-bound, every absolute time here is
> close to twice what a boosted core gives; the speedup columns are much less
> affected. Re-take on a boosted core before quoting the nanoseconds.

The crossover is about a dozen elements. If the match is at index 0 the
scalar version wins outright — it compares once and returns, while the
vector version still loads and tests a whole register. Past that, 2–3.6x.
This holds up across `-O2`, `-O3`, `-march=native`, and gcc — not an
artifact of one flag combination. The arm64 build runs the NEON versions
instead; every number in this file is from the Zen 5.

---

# The fix: Highway

Hand-writing intrinsics for one instruction set means the routine only runs
on that architecture. [Google Highway](https://github.com/google/highway)
is a portable SIMD library — the same source compiles to NEON, SVE, AVX2,
AVX-512, and RISC-V V. It is already used in [../sorting/](../sorting/).

It ships this exact algorithm:

```c
long find_highway(const int64_t* data, long n, int64_t target) {
  const hn::ScalableTag<int64_t> d;
  size_t pos = hn::Find(d, target, data, (size_t)n);
  return pos == (size_t)n ? -1 : (long)pos;   // Find returns count when absent
}
```

`hn::Find` is the same loop shape as the hand-written version — whole
vectors while they fit, then the remainder — written against an abstract
vector type. The interesting declaration is `ScalableTag<int64_t>`:
*however many int64 lanes this target has*. Two on NEON, four on AVX2, eight
on AVX-512, and on SVE or RISC-V V a width that is not known until the
program runs. `Lanes(d)` is not a compile-time constant, and the code never
needs it to be.

## What Highway costs on Zen

Little, which the `highway` column of the table above shows directly: it
tracks the hand-written version everywhere except the 8191 row, where it gives
up about 20%. The generated inner loops explain why. Hand-written:

```asm
vpcmpeqq  ymm1, ymm0, ymmword ptr [rdi + 8*rcx]
vmovmskpd r8d, ymm1
test      r8d, r8d
je        .LBB1_1
```

Highway, from portable source:

```asm
vpcmpeqq  ymm2, ymm1, ymmword ptr [rdi + 8*rcx - 32]
vmovmskpd edx, ymm2
test      edx, edx
je        .LBB2_1
```

The abstraction compiles away completely — Highway is a set of thin inline
wrappers over the platform's native vector instructions, so `ScalableTag`
compiles down to exactly the instructions a hand-written version would use.
The portability is paid for at build time and in a dependency, not in
cycles. It also writes the fiddly parts for you: remainder handling, and
turning a comparison mask into a lane index — the two places a hand-written
version is easiest to get wrong.

## One trap

Highway picks its instruction set from the `-m` flags, not from the CPU it
is running on. `-mavx2` alone is **not** enough to select the AVX2 target —
that target also requires BMI2, FMA, and AES — so a bare `-mavx2` silently
falls back to SSSE3 and runs at half width:

```
$ clang++ -O3 -mavx2 ... && ./find_first
highway target: SSSE3, 2 lanes per vector      <-- half the lanes

$ clang++ -O3 -mavx2 -mbmi -mbmi2 -mfma -mf16c -maes -mpclmul ... && ./find_first
highway target: AVX2, 4 lanes per vector
```

The program prints the selected target for this reason. If you benchmark
Highway against hand-written AVX2 without checking this line, Highway looks
about twice as slow as it is.

`-march=native` sets every flag Highway checks for, but on this machine it
overshoots. Zen 5 has AVX-512, so Highway takes the widest target it can:

```
$ clang++ -O3 -march=native ... && ./find_first
highway target: AVX3_DL, 8 lanes per vector
```

That is a fair build, but it is not the comparison in this file. Highway on
AVX-512 against hand-written AVX2 puts 8 lanes against 4. To compare like with
like, use the explicit flag list above. (`-march=znver5` is not an option with
this toolchain: clang 18 and g++ 13 do not recognize it.)

Two flags that look like they should work do not. `-march=x86-64-v3` — the
generic "AVX2-generation" portability level — omits AES and PCLMUL, so it
still falls back to SSSE3. So, surprisingly, does `-march=haswell`: real
Haswell silicon has AES-NI, but clang's target definition for it does not
set `__AES__`. Verified by dumping predefined macros for each target
(`clang++ -march=<x> -dM -E -x c++ /dev/null`) rather than trusting the
flag's name — only a concrete, current CPU model reliably sets all six.

---

# Apple silicon: two conclusions that do not carry over

Everything above is Zen 5. The same source on an Apple M5 gives a different
answer to two separate questions, and the second one is the more useful.

**Method for both.** Apple M5 performance core, macOS 26.6.2, Apple clang 17.0.0,
Highway 1.3.0, `-O3`, pinned and warmed, **measured at 4.44 GHz**, minimum of
seven passes, **median of five independent runs**. Every cell below is stable
across those five to under 1% except where called out.

```bash
clang++ -O3 -std=c++17 -stdlib=libc++ -I/opt/homebrew/include \
    -L/opt/homebrew/lib find_first.cpp -o find_first -lhwy
clang++ -O3 -std=c++17 -stdlib=libc++ -I/opt/homebrew/include \
    -L/opt/homebrew/lib lanes.cpp -o lanes -lhwy
```

Those two `-I`/`-L` flags are needed on macOS whenever Highway came from
Homebrew — clang does not search `/opt/homebrew` on its own, and without them
the build fails at the `#include <hwy/highway.h>`. The arm64 build line at the
top of this file needs them too.

## 1. Highway does not match hand-written NEON

On Zen, Highway and the hand-written version compile to the same instructions
and run at the same speed. On the M5 they do not — Highway wins at every row
past index 0:

| first match at | scalar | simd | highway | simd | highway | Highway's margin |
|---|---:|---:|---:|---:|---:|---:|
| 0 | 0.2 | 0.9 | 0.9 | 0.27x | 0.27x | — |
| 16 | 4.3 | 3.2 | 2.7 | 1.31x | 1.59x | **19%** |
| 256 | 65.6 | 49.2 | 40.4 | 1.33x | 1.62x | **22%** |
| 4096 | 927.6 | 751.3 | 518.0 | 1.23x | 1.79x | **45%** |
| 8191 | 1848.4 | 1495.8 | 1109.1 | 1.24x | 1.67x | **35%** |
| no match | 1847.8 | 1488.1 | 1102.1 | 1.24x | 1.68x | **35%** |

The Highway column is the only noisy one: at 8191 it ranged 1021–1112 ns over
the five runs, which is why that row's margin reads below the 4096 row's.

The cause is an instruction x86 has and NEON does not. AVX2 has `movemask`, so
both versions turn a lane comparison into one bitmask and one `ctz` — the same
four instructions, hence the tie on Zen. NEON has no equivalent, so the
straightforward hand-written port checks its two lanes with a branch each:

```c
uint64x2_t eq = vceqq_s64(v, wanted);
if (vgetq_lane_u64(eq, 0)) return i;        // a branch per lane
if (vgetq_lane_u64(eq, 1)) return i + 1;
```

Highway's `FindFirstTrue` for NEON instead packs the mask into 4-bit nibbles
and locates the lane with a single trailing-zero count, with no data-dependent
branch per lane (see `arm_neon-inl.h`). So the usual trade is inverted: the
portable library beats the hand-written intrinsics, and it beats them *because*
the architecture lacks the instruction the naive approach wants.

## 2. Speedup tracks lane count almost exactly

This is the question [lanes.cpp](lanes.cpp) exists to answer. A vector register
is a fixed number of bits, so the only way to change the lane count without
changing the machine is to change the element width — 128-bit NEON holds 2
`int64` or 16 `int8`. Highway's `ScalableTag<T>` gives the native lane count for
each, so one source covers every row.

The footprint is held at **64 KiB for every width**, not the element count. Fix
the count instead and the `int8` row touches 8 KiB while the `int64` row touches
64 KiB, and you are measuring the cache as much as the lanes.

| type | lanes | elements | scalar ns | highway ns | speedup | speedup at elem 16 |
|---|---:|---:|---:|---:|---:|---:|
| `int64` | 2 | 8192 | 1847.6 | 1011.0 | 1.83x | 1.58x |
| `int32` | 4 | 16384 | 3690.4 | 1006.6 | 3.67x | 2.38x |
| `int16` | 8 | 32768 | 7369.1 | 990.0 | 7.45x | 3.45x |
| `int8` | 16 | 65536 | 14738.4 | 932.9 | **15.80x** | 4.13x |

Speedup per doubling of lanes: **2.01, 2.03, 2.12**. Perfect scaling would be
2.00 each time.

Look at the two time columns rather than the speedup, because that is where the
result actually comes from. **The Highway column is flat** — 1011, 1007, 990,
933 ns — while the scalar column doubles every row. Both are consequences of
the same fact: the vector path's cost is per *byte* and the scalar path's cost
is per *element*. Sixteen `int8` lanes and two `int64` lanes move one register
per instruction either way, so scanning 64 KiB costs the vector loop the same
regardless of width; the scalar loop pays for each element it visits, and a
narrower type means more of them in the same 64 KiB.

So the speedup is not really a property of the SIMD unit at all — it is the
ratio of per-element work to per-byte work, and the lane count is what sets it.
That is why the scaling slightly *exceeds* 2.00 per doubling: Highway gets a
little faster at narrow widths (933 against 1011 ns), because the tail handling
and the per-vector bookkeeping are amortised over more elements.

The last column is the counterweight. With the match at element 16 there is
almost no work to amortise the vector path's fixed setup over, and the same
lane sweep yields only 1.58x → 4.13x instead of 1.83x → 15.80x. Wider lanes
still help, but the fixed cost never scales away — which is the index-0 row of
the first table, generalised.

**The ceiling and the measurement, once more.** The specification says 16
`int8` lanes and delivers 15.80x, while 2 `int64` lanes deliver 1.83x. That is
the clearest form of the lecture's claim in this directory: the lane count sets
the ceiling, and how close you get depends on how much per-element work you
gave the machine to remove.
