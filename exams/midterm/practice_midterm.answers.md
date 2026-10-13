# Midterm Practice: Answers

## Problem 1

**(a)** 1 → 2 (`x`), 2 → 3 (`y`), 4 → 5 (`z`).

**(b)** An instruction that starts in cycle *t* with latency *L* has its result
ready in cycle *t + L*.

| Instruction | Waits for | Starts | Result ready |
|---|---|---|---|
| 1. `x = a + b` | — | 1 | 2 |
| 2. `y = x * 2` | `x`, ready in 2 | 2 | 5 |
| 3. `w = y + 1` | `y`, ready in 5 | **5** (stalls 2 cycles) | 6 |
| 4. `z = c + d` | in order, after 3 | 6 | 7 |
| 5. `v = z * 2` | `z`, ready in 7 | 7 | 10 |

Instruction 3 stalls 2 cycles waiting for the multiply. Instruction 4 starts
late because it is stuck behind 3, even though it depends on nothing. All
results are ready at the end of **cycle 9**.

**(c)** Order 1, 2, 4, 5, 3:

| Instruction | Starts | Result ready |
|---|---|---|
| 1. `x = a + b` | 1 | 2 |
| 2. `y = x * 2` | 2 | 5 |
| 4. `z = c + d` | 3 | 4 |
| 5. `v = z * 2` | 4 | 7 |
| 3. `w = y + 1` | 5 | 6 |

No stalls. All results are ready at the end of **cycle 6**, against 9. The
independent work (4 and 5) fills the cycles that 3 would have spent waiting.

To do this safely, an out-of-order core needs the **dependencies**: which
instruction reads which earlier result. The order the code was written in only
matters where there is a RAW dependency, so it can move 4 and 5 ahead of 3
because none of them reads what the others write.

## Problem 2

| Loop | Carried from one iteration to the next? | Fewest cycles per iteration |
|---|---|---|
| (a) `b[i] = 2 * a[i] + 1` | **No.** Each iteration reads only its own `a[i]`. | no RAW limit; set by how fast the core can issue |
| (b) `x = x * a[i]` | **Yes, through `x`.** Each multiply needs the previous one's result. | **3** (the multiply latency) |
| (c) `c[i] = a[i] + a[i-1]` | **No.** See (iii). | no RAW limit |
| (d) `p[i] = p[i-1] + a[i]` | **Yes, through `p`.** Each sum needs the previous sum. | **1** (the add latency) |

**(iii)** Loop (c) reads `a[i-1]`, which an earlier iteration also read, but no
iteration ever **writes** `a`. Two reads of the same value are not a
dependency: both can happen at any time, in any order. A RAW dependency needs
a write followed by a read.

Worth pointing out: in (b), the chain can be broken with several independent
accumulators, because multiplication is associative (Activity 1). In (d), each
output is itself a running total, so that simple split does not work.

## Problem 3

One arrow: **4 → 11**, through `x2` (`s`). See
[raw_hazards.key.svg](raw_hazards.key.svg).

The add to `s` in one iteration must wait for the add to `s` in the previous
iteration. Nothing else stalls:

  * The loads depend only on `i`, never on the previous iteration's results, so
    the core issues them early and their latency is hidden.
  * The multiply feeds the add, but each product is computed fresh from new
    loads, so it is not on the chain from one iteration to the next.
  * Loop control (instructions 5–7 and 12–14) is predicted and is not a hazard.

Students who learned hazards on an in-order pipeline may also draw the
load-use arrows (1→3, 2→3) and the multiply-to-add arrow (3→4). Those are real
stalls on an in-order pipeline, but not here.

## Problem 4

**(a)** At `-O0`, every variable is stored to memory and reloaded between
statements. This extra memory traffic takes up much of the time
in both versions, so removing the RAW stall changes the total less. At `-O1` the
variables live in registers, and the dependency chain becomes most of the
cost.   `-O0` is important for degbugging. Each statement must write its
results to memory and the next must read them back, or a variable shown in the
debugger would be stale.

**(b)** No. The one-chain code is identical at `-O1`, `-O2` and `-O3`, and so
is its time. The compiler is **allowed** to split the chain, because unsigned
multiply is associative, but it does not. Allowed and actually done are
different things: breaking up a dependency chain is the programmer's job.

Contrast a floating-point sum: there the compiler is not even allowed to split
the chain, because floating-point addition is not associative, unless you give
it permission with `-ffast-math`.

**(c)** Every multiply needs the previous multiply's result, so there are no
independent multiplies to put side by side in vector lanes. (Also true: NEON
has no 64-bit integer multiply instruction, Activity 1 Part 3c, so there was
nothing to vectorize with.)

## Problem 5

Measured on Apple M5 at `-O1`, with a compiler barrier in the taken path to
keep each `if` a real branch. A loop that counts every element, with no `if`,
takes 7.5 ms.

**(a)**

| | `TEST` | Taken | Mispredicted |
|---|---|---|---|
| 1 | `i % 2 == 0` | 50% | almost never: it alternates taken, not taken |
| 2 | `x[i] % 2 == 1` | 50% | **about half the time** |
| 3 | `x[i] < 250` | about 98% (250 of 256 values) | about 2% |
| 4 | `i < n / 2` | 50% | about once, at the halfway point |

Test 2 is the one the predictor gets wrong often. Whether a random value is odd
has nothing to do with the values before it, so there is no pattern to learn.

**(b)** A predictor learns from **patterns** in a branch's recent history, not
from how often the branch is taken. Test 1 follows a fixed pattern (taken, not
taken, taken, ...), which the predictor learns and then gets right every time.
Test 2 depends on random data, so its history says nothing about the next
outcome: a coin flip. Taken 50% of the time does not mean unpredictable.

**(c)** About 16 million mispredictions (half of 32 million) × 20 cycles = 320
million cycles. At 4 × 10⁹ cycles per second, that is **80 ms**.

**(d)** Very close: test 2 takes 79 − 7.5 ≈ **72 ms** longer than tests 1 and
4, against an estimate of 80 ms. That is about 20 cycles per misprediction on
this machine.

Test 3 is right 98% of the time, but 2% of 32 million is still about **750,000
mispredictions** (6 values out of 256 are 250 or more). At about 20 cycles
each, that is a few milliseconds, which matches the extra 5.5 ms. A rare
misprediction is still expensive when the loop runs millions of times.

## Problem 6

Measured on Apple M5 at `-O1`, with a compiler barrier in the `if` version to
keep it a real branch. With 16 features and balanced classes: `if` version
4.7 ms, branch-free 7.1 ms.

**(a)** About **90%** of the time. The predictor learns that the branch is
usually not taken and guesses "not `c`", which is right for 9 samples in 10.
Mispredictions are rare, so the branch costs little, and the `if` version
skips the update for 90% of the samples. The branch-free version does all the
updates and gains nothing from removing a branch that was rarely wrong. That is
why it takes about 3 times as long. Its time barely changes between the two
data sets.

**(b)** With balanced classes in random order, the predictor is wrong about
half the time, and each misprediction costs about 20 cycles: more than the
4-feature update the `if` version skips. The branch-free version never
mispredicts.

**(c)** The **`if` version** (measured 4.7 ms against 7.1 ms). With 16 features
the update costs more than a misprediction, so skipping half the updates saves
more than the mispredictions cost.

The rule: branch-free code is a trade. It wins when the branch is unpredictable
and the work it does needlessly is cheap. It loses when the branch is
predictable, as with skewed classes, or when the work it does needlessly costs
more than a misprediction.

## Problem 7

**(a)** Each iteration is about 25 instructions (the lookup, the add, loop
control and the 20 in `f`), so about **24 iterations** fit in 600 entries. Their
lookups are all in the buffer together, and since they do not depend on each
other, the core sends them all to memory at once. The ~500-cycle waits
**overlap**, so the loop pays the memory latency only about once per 24
lookups.

(In practice another limit caps this too: a core can track only a few dozen
outstanding cache misses. Not required.)

**(b)** The lookup that misses sits at the head of the buffer, waiting. The
core keeps going and fills the buffer with `f`'s instructions behind it, but
nothing can retire until the lookup at the head completes. After about 600
instructions, all of them from this one call to `f`, the buffer is full and
fetching stops. The next iteration's lookup is about 2,000 instructions away,
so it never gets into the buffer while the first miss is outstanding. Only
**one** lookup waits for memory at a time, and every iteration pays the full
~500 cycles.

The two lookups are just as independent as in (a). What changed is the
**distance** between them: 2,000 instructions instead of 25, more than the
600-entry window.

**(c)** Separate the lookups from the calls, so the lookups are close together
again. For example, split the loop in two (loop fission):

```
for i in 0 .. n-1:
    total = total + table[idx[i]]    # lookups only: many in the buffer at once
for i in 0 .. n-1:
    f(i)
```

This is legal because `f` does not use the loaded values. Accept any answer
that puts several lookups within a few hundred instructions of each other.

## Problem 8

**(a)** Measured with the class benchmark (`examples/memory_hierarchy`,
`latency.csv`, AMD Zen 5 at about 5 GHz). Accept anything of the right order of
magnitude; the point is the ratios.

| Where the value is | Size | Latency (cycles) |
|---|---|---|
| register | a few bytes | **0** extra: it is ready for the next instruction |
| L1 cache | 48 KB | **4** |
| L2 cache | 1 MB | **14** |
| L3 cache | 16 MB | **about 70** |
| memory (DRAM) | many GB | **about 500** |

Each level is roughly 4–7 times slower than the one above it, and memory is
about 130 times slower than L1.

**(b)**

  * **Example 1: register, 0 extra cycles.** `count` is used on every
    iteration, so the compiler keeps it in a register (at `-O1` and above). The
    add itself takes 1 cycle. In the assembly, `inc rax` reads and writes a
    register and nothing else: there is no memory access at all.
  * **Example 2: L1, about 4 cycles.** The table is only 16 values (128 bytes
    of doubles, two cache lines). It is read again and again, so after the
    first few iterations it stays in L1. A **repeated reference** to the same
    small data hits in the fastest cache. In the assembly, the read is the
    `movsd` from `[rdx + 8*r8]`, a load from memory that finds its line in L1.
  * **Example 3: memory, about 500 cycles.** Each node is a **new
    reference**: it has not been read before, so it is not in any cache. The
    nodes are scattered at random, so the prefetcher cannot fetch them ahead.
    And each `p.next` depends on the previous read, so the reads cannot
    overlap: every one pays the full trip to memory. In the assembly, the
    `mov rdi, [rdi + 8]` loads the next address into the same register it
    reads from, so the next iteration's loads cannot start until it finishes.

## Problem 9

Measured on Apple M5 at `-O1`: 0.53, 0.82 and 2.9 ns per load. The data is
read once through before each timed run, so the timed loads start from memory,
not cache.

**(a)** The **hardware prefetcher** watches the addresses the loads ask for.
Pattern 2 moves forward by the same distance every time (64 bytes, a constant
stride), so the prefetcher predicts the next lines and fetches them before the
loop asks for them. Most loads find their line already in cache. Pattern 3's
next address cannot be predicted from the earlier ones, so the prefetcher
cannot help, and each load waits for its line to come from memory.

The point to take away: the prefetcher recognises **constant strides**, not
just "the next line".

**(b)** Pattern 1 uses all 8 doubles in each 64-byte line, so it needs only one
line from memory for every 8 loads. Pattern 2 uses one double per line and
needs a new line on every load: 8 times as many lines for the same number of
loads. Both are prefetched, so the difference is the amount of data moved.
