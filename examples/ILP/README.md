# Instruction-level parallelism

## Overview

Instruction-level parallelism (ILP) is the parallel execution of instructions
*within* a single serial thread. It is not concurrency — there is one
instruction stream — but a processor still completes more than one
instruction per cycle by overlapping and reordering the work inside that
stream. Four techniques make this possible:

* **Instruction pipelining** — an instruction executes in stages (fetch,
  decode, execute, ...), and independent instructions overlap those stages
  instead of running start-to-finish one at a time.
* **Out-of-order execution** — the hardware issues each instruction as soon
  as its inputs are ready, not in program order. The limit is the data
  dependency graph, not the instruction sequence: a chain where every
  instruction reads the previous one's result can't be reordered around.
* **Speculative execution** — at a branch, the CPU predicts which way
  control will go and keeps executing down that path rather than stalling
  until the condition resolves. A correct guess costs nothing; a wrong one
  discards everything issued past the branch and restarts.
* **Vector processing (SIMD)** — a single instruction operates on several
  data elements packed into one register at once. This is the one technique
  with a direct programming interface (compiler auto-vectorization or
  intrinsics); see [../vectorization/](../vectorization/).
  
  Pipelining, out-of-order execution, and speculation, by contrast, are
  managed by the hardware — you don't call them directly. But how you write
  code still determines how well the hardware can use them: a dependency
  chain, a loop boundary, or an unpredictable branch each hide available
  parallelism from the processor, and restructuring the code is often
  enough to expose it. That's what the three examples below measure.

Throughput is usually reported as **cycles per instruction (CPI)** — clock
cycles divided by instructions retired — or its reciprocal, **instructions
per cycle (IPC)**. One instruction per cycle is not a given; it's a ceiling
that data dependencies, stalls, and mispredictions pull you away from, and a
wide out-of-order core with enough exposed parallelism can beat it (CPI < 1)
by retiring several instructions in the same cycle — see the CPI column in
[speculative_execution.md](speculative_execution.md) for a measured example.
Simple instructions typically cost about a cycle and complex ones (integer
division, for instance) cost tens of cycles; for exact per-instruction
latencies on real hardware, see Agner Fog's
[instruction tables](https://www.agner.org/optimize/instruction_tables.pdf).

## The reorder buffer

All three examples below are really about one piece of hardware, so it is
worth setting out before you read them.

An out-of-order core decouples the order it *fetches* instructions from the
order it *executes* them. The structure that makes this safe is the **reorder
buffer (ROB)**: a queue of every instruction currently in flight. Each
instruction moves through three distinct steps:

1. **Enter, in program order.** The front end fetches and decodes, and
   allocates a ROB entry. Order here is the order you wrote.
2. **Execute, in dependency order.** An instruction issues to a functional
   unit as soon as *its inputs are ready* — which may be long before earlier
   instructions in the buffer have run. This is where reordering happens, and
   the only thing constraining it is the **data dependency graph**.
3. **Retire, in program order again.** Results become architecturally visible
   strictly in order, from the head of the buffer. This is what makes the
   whole scheme recoverable: if something ahead of an instruction faults, or a
   branch turns out to have been mispredicted, everything behind it in the
   buffer is discarded before it was ever official.

Two properties of the ROB explain everything the examples measure.

**It reorders by dependency, not by distance.** The hardware does not care
that you wrote `s += a[i]` before `s += a[i+1]`; it cares that the second one
reads the register the first one writes. Give it a chain and there is nothing
to reorder — it will issue one instruction per dependency latency no matter
how many idle execution units are sitting there. Give it independent work and
it will fill those units without being asked.

**It is finite, and therefore near-sighted.** The buffer holds a few hundred
instructions — Apple publishes no figure, but reverse-engineering of its
performance cores puts them in the 600s. Independent work that sits *inside*
that window gets interleaved automatically. Independent work that sits
outside it may as well not exist: the hardware never has both pieces in view
at the same time, so it cannot discover that they are independent. Note this
is a limit of *sight*, not of permission — a common and wrong intuition is
that some construct (a loop boundary, say) forbids reordering across it.
Nothing forbids it; the window is just too small to span 64M iterations.

Speculation is what keeps the buffer full in the first place. The front end
cannot wait for a branch to resolve before fetching past it — that would empty
the ROB at every conditional — so it predicts and keeps going, and the
in-order retire step cleans up when the prediction was wrong.

That gives two distinct ways for code to starve the machine, one per
example:

| What the code does | What the ROB sees | Example |
|---|---|---|
| independent work 10⁸ instructions away | never both in view | 1 · loop fusion |
| unpredictable branch | fetches the wrong path, discards it | 2 · speculation |

Each example measures one row, and the fix in both is a source change that
hands the ROB something it can work with.

A third effect — how far breaking a dependency chain scales, and what stops it
— lives in [../pipeline/multiple_accs.md](../pipeline/multiple_accs.md)
instead. That transformation needs no reorder buffer at all, since an in-order
machine issues the independent adds back to back just as happily, so it belongs
with the pipeline material. Read it first if you have not: it establishes the
saturation and bandwidth limits these two build on.

## The examples

Two examples, each isolating one source of ILP a modern out-of-order CPU can
exploit — or fail to, when the code gets in its way. All measurements are on
Apple M5 with Apple Clang 17; see each write-up for build flags and exact
numbers.

Each example is a single source file; build with `clang++ <flags> -o <name>
<name>.cpp` and run it directly — no shared Makefile. Example 1 builds plainly
at `-O1`; example 2 needs two extra `-mllvm` flags to stop the compiler from
optimizing away the very branch it is trying to measure — see its write-up.

Note that on macOS `g++` is a Clang shim, not GCC: `g++ --version` prints
"Apple clang version". Recipes here use `clang++` to make that explicit.

---

### 1 · Loop fusion and the reorder window

**This is the example that needs out-of-order hardware.** Two accumulation
loops compute independent reductions over the same array — the processor
*could* interleave them and fill each chain's stalls with the other's work. It
doesn't, and the reason is the useful part: 64M iterations of pass 1 is
~4 × 10⁸ instructions, and the reorder buffer holds a few hundred, so pass 2
sits about 700,000 windows away. Fusing the loops moves it one instruction
away.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [fuse_loops_rob.md](fuse_loops_rob.md) | that the ROB is near-sighted rather than blocked at a loop boundary, how big the window actually is, and why latency only costs you on the critical path |
| Run  | [fuse_loops_rob.cpp](fuse_loops_rob.cpp) | `sum` and `sumsq` over 64M floats — two passes, fused single-pass, fused with 2 accumulators per chain |

Fusing exposes the two chains' independence: 90.1 ms → 52.6 ms (**1.71x**);
doubling accumulators per chain removes the remaining stalls: 52.6 ms → 28.5 ms
(**3.16x** total). The working set stays latency-bound throughout — unlike
example 1, this one never reaches the bandwidth wall.

---

### 2 · Speculative execution — the cost of guessing wrong

A branch the CPU can't predict flushes 15+ cycles of speculative work on
every miss. Sorting the data (or removing the branch entirely) removes the
guesswork.

| Step | Open | What it teaches |
|------|------|------------------|
| Read | [speculative_execution.md](speculative_execution.md) | why sorted vs. shuffled data isolates prediction accuracy from instruction count, and why Apple Clang's automatic `csel` conversion hides the effect at `-O1` |
| Run  | [speculative_execution.cpp](speculative_execution.cpp) | a threshold filter over 32M ints — branchy+shuffled, branchy+sorted, branchless |

Sorting turns ~50% misprediction into one mispredict total: 83.1 ms → 7.7 ms
(**10.85x**); since the sorted and shuffled runs execute the same binary over
the same values, that entire speedup is CPI. The branchless version gets
**9.92x** — nearly all of the win, but not quite a tie: `csel` is cheap, not
free. The measured misprediction
penalty works out to ~18 cycles, somewhat above the 15 the source assumes.

---

### Files at a glance

**Sources** — `fuse_loops_rob.cpp`, `speculative_execution.cpp`
**Write-ups** — `fuse_loops_rob.md`, `speculative_execution.md`
**Assembly** — `fuse.s`, `spec.s` — reference disassembly used in the write-ups
**Binaries** — `fuse_O1`, `spec_O1` are build artifacts, gitignored rather than
shipped; build them with the command in each write-up. Anything already sitting in this
directory may predate the current harness — rebuild before trusting its output.