# Branch optimizations

Three examples about conditional branches: one technique worth learning even
though compiler typically optimizes it out and two cases where the compiler leaves the
work to you.

## Background: what is a branch miss?

A **branch** is any instruction where the CPU has to pick which instruction
runs next — an `if`, a loop condition, a `switch`. Modern CPUs are
**pipelined**: they don't run one instruction start-to-finish before starting
the next. Instead, dozens of instructions are in flight at once, each partway
through stages like fetch, decode, and execute. This only works if the CPU
knows *which* instructions to fetch several steps ahead of actually
evaluating the branch — but the branch's outcome (true or false) isn't known
until it executes, which is late in the pipeline.

The CPU's solution is **branch prediction**: a piece of hardware guesses
which way the branch will go — based on that branch's history — and the
pipeline starts fetching and executing instructions from the guessed path
*before* the branch is actually resolved. This is called **speculative
execution**. If the guess is right, the CPU gained a head start for free.

If the guess is wrong — a **branch misprediction** — every instruction that
was speculatively started down the wrong path has to be thrown away, and the
pipeline restarts from the correct path. This is a **pipeline flush**, and it
costs a fixed penalty of roughly 15–20 cycles on modern CPUs (measured at
≈15 on Apple M-series, ≈15–20 on x86). That penalty is paid in full no matter
how simple the branch itself is — an `if (x > 0)` mispredicted costs the same
15+ cycles as a mispredicted branch guarding a hundred lines of code.

**Why it matters:** branch predictors are excellent at finding patterns —
loops with a fixed trip count, branches that are almost always taken, `if`
statements on sorted data. But when the outcome is effectively random (as
with unpredictable input, hashing, or data-dependent conditionals), the
predictor is right about as often as a coin flip, and every wrong guess costs
that full 15–20 cycle flush. For a branch inside a hot loop processing
millions of elements, that overhead can dominate total runtime — turning a
loop that should take a few milliseconds into one that takes over 100. The
three examples below are three different techniques for avoiding that cost.

## Three Examples

**1 · Branch-free arithmetic** replaces a data-dependent `if` with signed-shift
masks and arithmetic. On this machine you will rarely write it by hand — a
plain ternary compiles to `csel`, and the hand-rolled bit tricks produce
*byte-for-byte identical* code from `-O1` up. The techniques still matter,
because the hardware where branches hurt most is the hardware that has no
conditional select to rescue you: **SIMD lanes**, which cannot branch
independently and must compute both sides and blend; **GPUs and shaders**,
where a divergent branch serializes an entire warp; and **in-order cores**,
which have neither `csel` nor an out-of-order window to hide a mispredict in.
Learn the transformations on a machine where you can measure them, then carry
them to targets where nothing will apply them for you.

**2 · Lookup tables** replace computation with a precomputed answer — here,
eight rounds of polynomial division per byte collapsed into one indexed load.
No optimization level derives this, because getting there needs a theorem about
GF(2) linearity plus an initialization phase that did not exist in the original
program. The speedup is **flat at 3.34x from `-O1` through `-O3`**, which is
the signature of a transformation the compiler genuinely cannot reach.

**3 · Loop unswitching blocked by aliasing** shows a branch the compiler is not
allowed to remove. The loop-invariant condition is read through a pointer of the
same type as the output array, so the compiler cannot prove the loop body leaves
it unchanged — the test never leaves the loop, and the body never vectorizes.
The fix is a one-line hoist you can make and the optimizer cannot, worth
**2.4x–4.4x at `-O3`**.

Together: know the technique in 1 for when you leave this architecture, and
recognize 2 and 3 as the two shapes of "the compiler will not do this" —
it cannot *derive* the transformation, or it cannot *prove* it is legal.

Measured on Apple M5, Apple Clang 17.

Each example is a single `.cpp` file; build with `clang++ -O2 -o <name> <name>.cpp`
and run it directly — no shared Makefile.

---

## 1 · Branch-free arithmetic — when the branch is unpredictable

Replace a data-dependent `if` with arithmetic and bitwise ops so there is no
branch left to mispredict.  **This example is redundant with speculative execution but goes into more cost details.**

| Step | Open | What it teaches |
|------|------|-----------------|
| Read | [branch_free.md](branch_free.md) | signed-shift masks, branchless min/max, comparison-as-integer |
| Run  | [branch_free.cpp](branch_free.cpp) | clamp + threshold-count over 32M random bytes |

Random data defeats the branch predictor outright — branchless code turns a
121 ms misprediction-bound loop into a 5 ms straight-line one (~24x).

---

## 2 · Lookup table — when the computation is more than a peephole

Replace computation with a precomputed answer — and the harder question of when
that is actually a win.

| Step | Open | What it teaches |
|------|------|-----------------|
| Read | [lut_crc32.md](lut_crc32.md) | why a table beats an 8-round loop and loses to a two-way select, and how a flat `-O1`..`-O3` curve identifies a real optimization |
| Run  | [lut_crc32.cpp](lut_crc32.cpp) | CRC32 over 32M bytes: bitwise inner loop vs 256-entry table |

CRC32's table replaces eight data-dependent shift/xor rounds per byte, which no
optimization level can derive: **3.36x at `-O1`, 3.34x at `-O2`, 3.34x at
`-O3`** — flat, because the compiler has nothing to contribute. 36 instructions
per byte become 7.

The write-up gives four reasons the transformation is out of reach: it needs a
theorem about GF(2) linearity, a new initialization phase, an unbounded
space-time tradeoff, and it is blocked by a serial dependency. Meanwhile the
compiler does apply everything it has — full unroll, every ternary if-converted
to `csel` — and is still 3.34x behind.

Note this example needs **no compiler barrier**. Both versions are already
branchless, so the difference measured is work, not prediction.

---

## 3 · Loop unswitching blocked by aliasing — when invariance cannot be proven

A loop-invariant condition read through a pointer of the same type as the output
array. The compiler cannot prove the store leaves it intact, so the test never
leaves the loop and the body never vectorizes.

| Step | Open | What it teaches |
|------|------|-----------------|
| Read | [unswitch_alias.md](unswitch_alias.md) | why the selector's *type* decides whether the compiler can hoist it, and legality vs. budget as two different reasons an optimizer declines |
| Run  | [unswitch_alias.cpp](unswitch_alias.cpp) | `mode`-dispatched elementwise op over 32M floats, selector passed by pointer |

The manual hoist is one line — `float m = *mode;` — and it is worth **2.4x to
4.6x at `-O3`**, where the gap does not close. Removing the test is the small
part; making the body specialisable, and therefore vectorizable, is where the
speedup comes from.

---

## Files at a glance

**Sources** — `branch_free.cpp`, `lut_crc32.cpp`, `unswitch_alias.cpp`
**Write-ups** — `branch_free.md`, `lut_crc32.md`, `unswitch_alias.md`

Each write-up's Build section notes the `KEEP_BRANCH()` compiler barrier used
to keep the "branchy" baseline from being auto-optimized into branchless code
by the compiler itself — without it, `-O2`/`-O3` can rewrite the very branch
each example is trying to measure.
