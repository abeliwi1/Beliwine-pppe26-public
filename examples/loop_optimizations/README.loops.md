# Loop optimizations

Five classic loop transformations, each isolated in its own example so the
win (or the surprising non-win) is unambiguous. Measured on an AMD Ryzen AI 9
HX 370 (Zen 5, "Strix Point") under Linux with g++ 13.3 — 48 KB 12-way L1d
with 64-byte lines, 1 MB L2 per core, 16 MB L3, 5.13 GHz. See each write-up
for build flags and exact numbers.

Every benchmark pins itself to a fast core and warms the clock before
measuring. Both matter on this part: the chip has 5.13 GHz Zen 5 cores and
3.17 GHz Zen 5c cores, and the governor boosts on instructions-per-cycle, so a
memory-bound loop left to itself runs the whole way at ~3.59 GHz — and every
repetition is equally slow, so taking a minimum does not rescue it.

Each example is a single source file; build with `g++ <flags> -o <name>
<name>.cpp` and run it directly — no shared Makefile.

---

## Why loops

Most of the time a real program spends is spent inside a small number of
loops — the ones that touch every element of an array, walk a matrix, or
accumulate a sum. A program might run thousands of distinct lines of code
over its lifetime, but a handful of loop bodies account for nearly all the
actual work, because each one executes not once but thousands, millions, or
billions of times. Shave one cache miss or one wasted instruction off a
single iteration of a loop that runs a million times, and you've removed a
million cache misses or a million wasted instructions. That leverage — fix
it once, benefit every iteration — is why loops get disproportionate
attention compared to code that only runs once.

It's also *why loops are unusually amenable to a fixed toolkit*. A loop body
is the same operation applied to different data, iteration after iteration,
so a handful of general-purpose rewrites — reorder the iterations, split the
loop, merge two loops, do several iterations at once — tend to work across
completely unrelated problems: image filters, matrix multiplication,
running sums, string search. That's what this directory is: five such
rewrites, each demonstrated on the example where its effect is clearest.

Importantly, none of the five change *what* the loop computes — only how the
work inside it is arranged, so the compiler, the cache, and the CPU's
execution units spend less time waiting and more time working. Whether a
given rearrangement is even allowed — whether it's still guaranteed to
compute the same thing — comes down to one question, covered next.

---

## Loop independence — what makes a transformation legal

Every rewrite below reorders, splits, merges, regroups, or overlaps loop
iterations. That's only a valid rewrite — same answer, different code — if
the iterations are **independent**: iteration `i`'s computation doesn't
depend on any other iteration's result, and doesn't feed into one either.
Equivalently, there's no *loop-carried dependency* — no value written in one
iteration that a different iteration reads.

That's what each transformation actually needs independence for:

* **Fission** splits one loop into several — legal when nothing in the body
  spans the split point.
* **Fusion** merges two loops into one — legal when neither loop's iteration
  needs output the other loop hasn't produced yet at the point they'd now
  run interleaved.
* **Interchange** swaps which index is outer and which is inner — legal when
  neither index's loop feeds into the other's computation.
* **Tiling** reorders iterations into blocks — the same requirement as
  interchange, at a coarser grouping.
* **Unrolling** runs several iterations per pass through the loop-control
  code — legal for the same reason interchange is: overlapping iterations
  changes nothing if they don't depend on each other.

A loop where iteration `i` genuinely needs iteration `i-1`'s result — a
running sum, a recurrence, an accumulator — *is* loop-carried, and none of
these rewrites are free there. [../pipeline/multiple_accs.md](../pipeline/multiple_accs.md)
and [../ILP/fuse_loops_rob.md](../ILP/fuse_loops_rob.md) measure what a
loop-carried dependency costs on real hardware, and how breaking one apart
(multiple accumulators) or hiding it (interleaving independent work)
recovers performance without changing the loop's dependency structure.

**Independent isn't automatically "safe to reorder bit-for-bit," though.**
[loop_fusion.md](loop_fusion.md)'s mean/variance example makes the case: the
two-pass and fused versions both sum the same values in the same left-to-right
order — genuinely independent per-element work, combined by one reduction —
so fusing them doesn't touch the arithmetic. But floating-point addition
isn't associative, so the compiler won't auto-vectorize either version's
reduction without `-ffast-math`: packing several additions into one SIMD
instruction changes the order partial sums combine in, which can change the
last bit of the result. Welford's algorithm, by contrast, maintains a running
mean and variance that genuinely *is* loop-carried — each iteration needs the
previous iteration's mean — so it can't be reordered at all, and pays for
that with a serial dependency chain despite doing the fusion "for free" in a
single pass.

So there are really two questions hiding inside "independent": can this loop
be reordered at all (the dependency-graph question — what makes a
transformation legal), and does reordering it change the answer (the
floating-point-associativity question — why even a legal reordering needs a
flag like `-ffast-math` to happen automatically). Every example below has
settled the first question already; only loop fusion's FP reduction runs
into the second.

### See it move

[**loop_carried_dependency.html**](loop_carried_dependency.html) — two loops over
the same eight numbers, driven by one shared clock. The dependent loop passes a
baton (the running total `s`) from box to box and needs eight ticks; the
independent loop has no baton, finishes on tick one, and then sits idle for seven
ticks while the other is still crawling. The page is standalone — double-click it,
no server required.

It is the shortest way to make the legality argument above land before anyone has
looked at a benchmark.

---

## 1 · Loop unrolling — trade branch/loop overhead for code size

Process multiple elements per iteration to amortize the loop-control
overhead, and handle whatever doesn't divide evenly (the **tail problem**).

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [loop_unrolling.md](loop_unrolling.md) | why removing loop-control overhead returns nothing on an out-of-order core, and what to do instead |
| Run  | [loop_unrolling.cpp](loop_unrolling.cpp) | sum-reduction, four strategies × five working sets (8 KB → 64 MB) × three optimization levels |
| Port | [loop_unrolling.rs](loop_unrolling.rs) | the same three strategies in Rust — and why Duff's Device doesn't translate mechanically (no fallthrough in `match`) |

**Unrolling on its own does nothing here**: 1.01–1.04x at `-O1`, flat across
every working set from 8 KB to 64 MB, and Duff's Device is consistently ~5%
*slower*. The scalar loop already runs at 1.0 cycles/element — the latency of
the dependent chain through the accumulator — so loop control is issuing for
free and there is nothing to remove. What the unrolled body is good for is
making room for something else: four independent accumulators give **1.65x**
at `-O1`, and at `-O2` GCC vectorises the unrolled kernels (**2.03x**) while
declining to vectorise the scalar loop. Unrolling is an enabler, not a win.

---

## 2 · Loop fusion — trade compute for a memory pass

Merge two loops that scan the same data into one, when the data dependency
between them can be worked around algebraically.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [loop_fusion.md](loop_fusion.md) | using Var(X) = E[X²] − E[X]² to fuse mean+variance into one pass, and why the "textbook" single-pass alternative (Welford's) is *slower* |
| Run  | [loop_fusion.cpp](loop_fusion.cpp) | mean/variance over 50M ints — two-pass, fused, and Welford's, side by side |

Fusing removes a full cold-cache reread of a 381 MB array: 59 ms → 29 ms
(**2.02x**) — the 2x memory-bandwidth ceiling exactly, meaning the extra
multiply in the fused body is free. Welford's fuses the passes too but adds a
per-iteration division and a loop-carried dependency — 0.31x, *slower* than
doing two passes.

---

## 3 · Loop interchange — align the inner index with memory layout

Swap nested loop indices so the inner loop walks memory contiguously instead
of striding across rows.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [loop_interchange.md](loop_interchange.md) | row-major stride math, and why you count what must be *retained* rather than what is touched |
| Run  | [loop_interchange.cpp](loop_interchange.cpp) | matrix–vector multiply, column access vs. row access over a 128 MB matrix |

Reordering with no algorithmic change: stride-32KB column access (one miss per
element, 1 of 8 doubles per line used) vs. stride-1 row access — 67 ms → 11 ms
(**6.29x**), with identical results to the last bit.

---

## 4 · Loop fission — trade a memory pass for fewer live registers

Split a wide loop body into narrower loops to cut register pressure and
eliminate spills to the stack.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [loop_fission.md](loop_fission.md) | why 32 simultaneous accumulators exhaust x86-64's 16 XMM registers, and how to count spills in the generated assembly |
| Run  | [loop_fission.cpp](loop_fission.cpp) | 32 simultaneous cross-correlations, split 1/2/4/8 ways |

Fissioning trades extra reads of the signal for the removal of per-iteration
spills: 238 ms → 56 ms (**4.28x**). The speedup tracks the spill count exactly
— 134 stack references at 32 accumulators per loop, 57 at 16, **zero at 8** —
so the knee is where the spills disappear, not where the arithmetic changes.

---

## 5 · Loop tiling — block the iteration space to fit a cache level

Restructure a loop into rectangular blocks so each block's working set is
reused from cache instead of reloaded from DRAM.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [loop_tiling.md](loop_tiling.md) | the no-reuse case: why the leading dimension, not the tile size, governs a transpose — and why padding beats tiling there |
| Read | [matmul_tiling.md](matmul_tiling.md) | the reuse case: why blocking wins ~3x on matrix multiply, and why padding cannot substitute |
| Run  | [loop_tiling.cpp](loop_tiling.cpp) | 4096×4096 transpose: tile size swept 2→256, then the same transpose with the row stride swept |
| Run  | [matmul_tiling.cpp](matmul_tiling.cpp) | matmul at n=512/1024/2048, blocked in one, two and three dimensions |

Tiling pays in proportion to how many times a kernel re-reads the same byte, so
that is the question to ask first. **Matrix multiply** reads every element of B
once per row of A — blocking cuts that traffic by the tile size and wins
**2.98x** at n=2048, growing with the problem. **Transpose** reads every element
exactly once, so there is no reuse for a tile to capture, and the answer comes
out the other way.

On the transpose: naive writes stride-`lda` (unpredictable for the prefetcher),
and tiling gets 188 ms → 34 ms (**5.5x** at B=16). But there the tile size is the
wrong variable. At `lda=4096` a column of the output reaches exactly **one** of the
64 L1 sets, and that set has 12 ways. Padding `lda` to 4104 makes the *naive*
loop 7.8x faster — **1.4x faster than the best tile size ever manages** — and
then tiling becomes a net loss. With no reuse, pad first; tile only if it still
doesn't fit. The two are independent transformations — on matmul, padding helps
the *blocked* version and does nothing for the unblocked one.

---

## Files at a glance

**Sources** — `loop_fission.cpp`, `loop_fusion.cpp`, `loop_interchange.cpp`, `loop_tiling.cpp`, `matmul_tiling.cpp`, `loop_unrolling.cpp`, `loop_unrolling.rs`
**Visual** — [loop_carried_dependency.html](loop_carried_dependency.html) — standalone cartoon contrasting a dependent and an independent loop on one clock
**Visual** — [loop_fission_spill.html](loop_fission_spill.html) — standalone panel showing the register files overflowing to the stack as the loop body widens
**Visual** — [loop_interchange_memory.html](loop_interchange_memory.html) — standalone cartoon showing row- versus column-order traversal against the memory the array actually occupies
**Visual** — [tiling_iteration_order.html](tiling_iteration_order.html) — standalone animation of blocked versus unblocked iteration order, counting the operands each keeps live
**Write-ups** — `loop_fission.md`, `loop_fusion.md`, `loop_interchange.md`, `loop_tiling.md`, `matmul_tiling.md`, `loop_unrolling.md`
**Reference** — [loop_optimizations_overview.md](loop_optimizations_overview.md) — non-benchmarked software examples of fusion and fission across domains (image processing, DB queries, compilers, video/audio, ML pipelines, network packet processing)

Every write-up here follows the same arc: state the transformation, show why
the *obvious* example is a poor fit (fusion tempts you toward Welford's,
tiling tempts you toward matmul), then land on the example that isolates the
real effect and measure it.
