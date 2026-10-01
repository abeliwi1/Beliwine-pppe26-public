# Activity 2 — Cache Set Conflicts: Predicting Which Array Sizes Are Slow

(co-authored by Claude and Codex)

## Background

A matrix transpose reads one row of the input left to right — stride 1, which
the hardware prefetcher handles easily — and writes one *column* of the output,
stepping a full row between consecutive writes. That write stride is where the
time goes, and how much time depends on a number you might not expect: the
**leading dimension**, which is how many elements one row occupies in memory.

The logical matrix here is always 2048 × 2048. What changes is `lda`, the row
stride. `lda = 2048` is the obvious, tight-packed choice. It is also, on most
machines, the worst one you could pick.

Here is the mechanism. Cache lines are assigned to sets by their address:

```
set index = (address / line_size) % number_of_sets
```

Stepping down one column advances the address by one row — `lda * 8` bytes for
doubles. In cache lines that is `lda * 8 / line_size`, so each step down the
column advances the set index by

```
adv = (lda * 8 / line_size) % number_of_sets
```

Repeatedly adding `adv` modulo the set count visits only some of the sets, and
possibly very few of them. When `adv` is 0, every element of the column lands in
**one single set** — and a set holds only `ways` lines. A 2048-element column
fighting over a 12-way set evicts itself continuously, from the top of the
column to the bottom, every time.

Your job is to turn that paragraph into a function that **predicts**, before
running anything, which leading dimensions will be slow.

## Getting started

Two files come with this assignment:

* [`activity2.loops.starter.cpp`](activity2.loops.starter.cpp) — the harness.
  Rename it to `activity2_conflicts.cpp` and work in it.
* This handout.

The harness already reads your machine's cache geometry from
`/sys/devices/system/cpu/cpu*/cache/`, pins itself to a core, warms the clock,
runs the transpose across ten leading dimensions, and prints a table. **You
write exactly one function.**

```bash
g++     -std=c++17 -O1 -o activity2 activity2_conflicts.cpp && ./activity2   # Linux
clang++ -std=c++17 -O1 -o activity2 activity2_conflicts.cpp && ./activity2   # macOS
```

It runs on x86-64 and on Apple silicon. If the program cannot work out your
cache geometry, supply it by hand and it will use what you give it:

```bash
L1_SIZE=131072 L1_WAYS=8 L1_LINE=128 ./activity2
```

macOS does not report associativity at all, so on a Mac the program measures
it — by chasing a pointer around W lines that all land in one set and finding
the W where the hops stop being L1 hits. It prints what it found.

> **Two things the harness does that you should not remove.** It pins to one
> core, because this machine has fast and slow cores and an unpinned run lands
> on whichever the scheduler picks. And it spins on a high-IPC loop before
> measuring, because the clock governor boosts on instructions-per-cycle, not on
> the core being busy — a memory-bound loop left alone runs the whole way at the
> base clock, and *every* repetition is equally slow, so taking the minimum of
> five does not save you. Both are why the program prints the clock it measured
> at, at the start and at the end.

## Part 1 — Read your machine

Run the starter as given and record what it prints for your machine:

| Quantity | Value |
|---|---|
| L1d size | ==---== |
| Associativity (ways) | ==---== |
| Line size (bytes) | ==---== |
| Number of sets | ==---== |
| Core clock after warm-up | ==---== |

**(a)** The harness computes `sets = size / (ways * line)`. Verify that
arithmetic by hand for your machine and show the calculation.

==---==

**(b)** How many doubles fit in one cache line on your machine?

==---==

## Part 2 — Predict, before you measure

Implement `sets_reachable(lda, g)` in your `activity2_conflicts.cpp`. It returns how many
distinct L1 sets one column of the matrix touches, given the leading dimension
and your machine's geometry.

```cpp
int sets_reachable(int lda, const Geometry& g) {
    // your code
}
```

Two cases are easy to get wrong, and both appear in the table below:

* A row that is **not a whole number of cache lines**. The set index no longer
  cycles through a fixed pattern — think about what it does instead.
* An advance of **exactly 0**. Do not let this fall out of your `gcd` as if it
  were an ordinary case; reason about what the column actually does.

Now **fill in the prediction column before you run the program.** This is the
graded part of the assignment, and predictions submitted after measuring are
worth nothing — the whole point is that the formula tells you the answer in
advance.

| `lda` | row bytes | sets reached (predicted) | fast or slow? (predicted) |
|---|---|---|---|
| 2048 | | ==---== | ==---== |
| 2049 | | ==---== | ==---== |
| 2050 | | ==---== | ==---== |
| 2052 | | ==---== | ==---== |
| 2056 | | ==---== | ==---== |
| 2064 | | ==---== | ==---== |
| 2080 | | ==---== | ==---== |
| 2112 | | ==---== | ==---== |
| 2176 | | ==---== | ==---== |
| 2304 | | ==---== | ==---== |

**(c)** State your rule for "fast or slow" as a threshold on sets reached, and
justify the number you picked using the associativity of your L1. How many
lines of a single column can be resident at once if the column reaches `k`
sets?

==---==

## Part 3 — Measure, and score yourself

Run the program and fill in the measured column:

| `lda` | sets reached | measured time | speedup vs `lda`=2048 | prediction correct? |
|---|---|---|---|---|
| 2048 | | ==---== | 1.00x | ==---== |
| 2049 | | ==---== | ==---== | ==---== |
| 2050 | | ==---== | ==---== | ==---== |
| 2052 | | ==---== | ==---== | ==---== |
| 2056 | | ==---== | ==---== | ==---== |
| 2064 | | ==---== | ==---== | ==---== |
| 2080 | | ==---== | ==---== | ==---== |
| 2112 | | ==---== | ==---== | ==---== |
| 2176 | | ==---== | ==---== | ==---== |
| 2304 | | ==---== | ==---== | ==---== |

**(d)** Sort your rows by sets reached rather than by `lda`. Is the time
monotonic in sets reached? Where does the curve flatten out — that is, past what
number of sets does reaching more sets stop buying you anything?

==---==

**(e)** `2304` is not a power of two, and it is one of the slowest rows in the
table. `2049` is odd and is one of the fastest. A classmate tells you the rule
is "avoid powers of two." Using your formula, explain in one or two sentences
what the actual rule is.

==---==

**(f)** You have a 2048 × 2048 matrix of doubles and you are allowed to waste
some memory on padding. What is the **smallest** `lda ≥ 2048` that reaches your
threshold from (c)? How much extra memory does that padding cost, as a
percentage? Verify by adding it to the `ldas[]` list and re-running.

==---==

## Part 4 — Does the rule transfer?

This machine is not one processor. `cpu0`–`cpu3` are Zen 5 cores; `cpu4`–`cpu11`
are Zen 5c "dense" cores with a lower clock. The harness reads `PIN_CPU` so you
can run the same binary on each:

```bash
PIN_CPU=2 ./activity2     # a Zen 5 core
PIN_CPU=6 ./activity2     # a Zen 5c core
```

On Apple silicon there is no way to pin to a numbered core, so the program asks
for a *cluster* instead via its quality-of-service class: `PIN_CPU` under 4
requests a performance core, 4 or above an efficiency core. Same experiment,
same two questions — a fast cluster and a slow one.

Run both and fill in:

| | Zen 5 (`PIN_CPU=2`) | Zen 5c (`PIN_CPU=6`) |
|---|---|---|
| Clock after warm-up | ==---== | ==---== |
| L1d size / ways / sets | ==---== | ==---== |
| Time at `lda` = 2048 | ==---== | ==---== |
| Best time in the table | ==---== | ==---== |
| Speedup, worst to best | ==---== | ==---== |

**(g)** Did your Part 2 predictions need to change at all for the second core?
Explain why or why not in terms of what the formula actually depends on.

==---==

**(h)** Compute two ratios: the slow core's time divided by the fast core's
time at `lda = 2048`, and the same ratio at your best `lda`. Then compute the
ratio of the two clock speeds. One of your two ratios is close to the clock
ratio and the other is much smaller.

Which is which, and what does that tell you about where the time goes in each
case? (Hint: a loop whose time scales with the core clock is limited by the
core. A loop whose time barely moves when the clock changes is limited by
something else.)

==---==

**(i)** The padding speedup is *larger* on the slower core. Is that consistent
with your answer to (h)? Explain.

==---==

## Part 5 — AI Use Disclosure

State whether you used AI tools for this assignment. If you did, name the tools
and briefly describe how you used them. Full credit is earned at every level of
use; the course was designed with AI as a partner in mind.

==---==

## What to submit

1. Your completed `activity2_conflicts.cpp`, with `sets_reachable` implemented.
   (30 points)
2. A document with your written answers:
   - 2a. Part 1 table and answers (a)–(b). (10 points)
   - 2b. Part 2 prediction table and answer (c). (20 points)
   - 2c. Part 3 measured table and answers (d)–(f). (20 points)
   - 2d. Part 4 table and answers (g)–(i). (15 points)
   - 2e. AI use disclosure. (5 points)

Raw terminal output is fine for the tables — screenshots or copy-pasted text
both work.

## RB Questions/Reflection

These are not required, but worth thinking about:

- Every element of the matrix gets read exactly once and written exactly once,
  no matter what `lda` is. The instruction count barely changes. Where does a
  4x difference in running time come from, if not from doing more work?
- Tiling is the textbook fix for a slow transpose, and it is real — but on this
  machine a padded naive transpose beats the best tile size. When would you
  still reach for tiling?
- Your formula depends on the line size, the set count, and the element size.
  If you switched from `double` to `float`, which rows of your table would
  change, and which would not?
