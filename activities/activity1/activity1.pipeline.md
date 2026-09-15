# Activity 1 — Pipeline Stalls: Independent Accumulators

(co-authored by Claude and Codex)

## Background

A pipelined CPU overlaps the stages (fetch, decode, execute, ...) of
consecutive instructions — but only if those instructions are independent.
When one instruction must read a value the instruction right before it just
wrote (a **RAW hazard: Read-After-Write**), the pipeline has to **stall**:
it idles with bubble cycles instead of doing useful work, waiting until that result is avaliable.

Below is a function with exactly this problem. A single accumulator `x` is
multiplied by one array element at a time, and every multiply reads the
value the multiply above it just wrote:

```cpp
unsigned long long noTempVars(vector<int>& data) {
    unsigned long long x = 1;
    for (int i = 0; i < ARRAY_SIZE; i += 4) {
        x = x * ((unsigned long long)data[i]     | 1ull);   // stall: reads x just written
        x = x * ((unsigned long long)data[i + 1] | 1ull);   // stall: depends on line above
        x = x * ((unsigned long long)data[i + 2] | 1ull);   // stall: depends on line above
        x = x * ((unsigned long long)data[i + 3] | 1ull);   // stall: depends on line above
    }
    return x;
}
```

Every one of the 100 million multiplies has to wait for the previous one
to finish. No matter how many multipliers the chip has sitting idle, this
loop can't go faster than multiply latency times 100 million.

Two details in that code are deliberate and you should keep both:

* **`| 1ull`.** This forces every multiplier odd. Without it, the running
  product accumulates factors of 2 until it hits zero after overflow and stays there —
  and a benchmark that multiplies zero by zero 100 million times isn't
  measuring anything interesting.
* **`unsigned long long`.** Unsigned arithmetic wraps around, and
  wraparound is *defined* behavior. If the behavior was undefined,
  it would make the measurement meaningless at overflow.

## Part 1 — Fix it

Your job is to fill in the code of `withTempVars` below. The idea: instead of one
accumulator that every multiply depends on, use **four** accumulators that
don't depend on each other at all. Split the loop body into two phases:

* **LOAD PHASE** — read four operands out of `data` into temporary variables.
  These depend on nothing but memory, so all four can proceed at once.
* **COMPUTE PHASE** — multiply each accumulator by its own temporary only.
  There are no cross-dependencies, so all four multiplies can overlap.

Then combine the four partial products into one return value — **once,
after the loop**, not inside it.

```cpp
/*
 * activity1_pipeline.cpp
 *
 * Compile and benchmark:
 *   clang++ -std=c++17 -O1 -o activity1 activity1_pipeline.cpp && ./activity1
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <climits>

using namespace std;
using namespace std::chrono;

const int ARRAY_SIZE = 100000000;

static void clobber() {
    asm volatile("" ::: "memory");
}

// ============================================================
// GIVEN: single accumulator, real RAW hazard
// ============================================================
unsigned long long noTempVars(vector<int>& data) {
    unsigned long long x = 1;
    for (int i = 0; i < ARRAY_SIZE; i += 4) {
        x = x * ((unsigned long long)data[i]     | 1ull);   // stall: reads x just written
        x = x * ((unsigned long long)data[i + 1] | 1ull);   // stall: depends on line above
        x = x * ((unsigned long long)data[i + 2] | 1ull);   // stall: depends on line above
        x = x * ((unsigned long long)data[i + 3] | 1ull);   // stall: depends on line above
    }
    return x;
}

// ============================================================
// YOUR TASK: split each loop body into a load phase and a compute
// phase, each internally independent, so the CPU can overlap all
// four multiplies of an unrolled iteration instead of serializing
// them one at a time.
// ============================================================
unsigned long long withTempVars(vector<int>& data) {
    // TODO:
    // Create four independent accumulators. Think about what value to
    // seed them with so the four partial products, multiplied together
    // at the end, equal what the single accumulator produced.


    for (int i = 0; i < ARRAY_SIZE; i += 4) {
        // TODO:
        // LOAD PHASE: four independent operands from data (remember | 1ull)


        // TODO:
        // COMPUTE PHASE: four independent multiplies

    }
    // TODO:
    // combine the four partial products into the single return value

}

// ============================================================
// Benchmark harness — do not modify
// ============================================================
template <typename Func>
pair<long long, unsigned long long> benchmark(Func func, vector<int>& data, int runs = 5) {
    long long minTime = LLONG_MAX;
    unsigned long long result = 0;

    for (int r = 0; r < runs; r++) {
        clobber();
        auto start = high_resolution_clock::now();
        clobber();

        // Accumulate, don't overwrite: with `result = func(data)` only the
        // last of the five calls' results is ever read, so the compiler may
        // delete the other four as dead code and the minimum below comes
        // from a run that never happened.
        result += func(data);

        clobber();
        auto end = high_resolution_clock::now();
        clobber();

        long long t = duration_cast<milliseconds>(end - start).count();
        minTime = min(minTime, t);
    }

    volatile unsigned long long dummy = result;
    (void)dummy;

    return {minTime, result};
}

int main() {
    cout << "=== Activity 1: Independent Accumulators ===" << endl;
    cout << "Array size: " << ARRAY_SIZE << " elements\n" << endl;

    vector<int> data(ARRAY_SIZE);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        data[i] = (i % 100) + 1;
    }
    clobber();

    // Warmup
    volatile unsigned long long w = noTempVars(data);
    w += withTempVars(data);
    (void)w;

    auto [timeA, resA] = benchmark(noTempVars,   data);
    auto [timeB, resB] = benchmark(withTempVars, data);

    cout << "One chain (stalled):        " << setw(5) << timeA << " ms" << endl;
    cout << "Four chains (pipelined):    " << setw(5) << timeB << " ms" << endl;
    cout << "Speedup: " << fixed << setprecision(2)
         << (double)timeA / max(1LL, timeB) << "x" << endl;

    cout << "Same answer? " << (resA == resB ? "YES" : "NO") << endl;

    volatile unsigned long long checksum = resA + resB;
    cout << "(Checksum: " << checksum << ")" << endl;

    return 0;
}
```

**Sanity check before you move on.** A correct `withTempVars`
given the supplied inputs prints:

```
Same answer? YES
(Checksum: 16783984258637683210)
```

That exact value, at every optimization level.
This check is meaningful here in a way it usually isn't. Your four-chain
version computes the array's product in a **different order** than the
one-chain version does — and it still must land on this identical value,
bit for bit. That works because 64-bit unsigned multiply is **associative
and commutative**: regrouping and reordering the factors cannot change the
answer.

That property is the whole license for this optimization. **Splitting a
dependency chain is only legal when the operation you're splitting is
associative.** If you tried the same trick on a chain of, say,
`x = (x + d) * 3 ^ (x >> 2)`, you would get a faster loop that computes a
different number — which is not an optimization, it's a bug. Whenever you
reach for this technique in real code, check the associativity first.

If your checksum between the methods differs, your rewrite isn't
computing the same thing as `noTempVars`, effectively nullifying
any meaningful intepretation of the speedup you measure because
you're timing different programs.

## Part 2 — Measure it across optimization levels

Build and run your program at all four optimization levels:

```bash
clang++ -std=c++17 -O0 -o activity1_O0 activity1_pipeline.cpp && ./activity1_O0
clang++ -std=c++17 -O1 -o activity1_O1 activity1_pipeline.cpp && ./activity1_O1
clang++ -std=c++17 -O2 -o activity1_O2 activity1_pipeline.cpp && ./activity1_O2
clang++ -std=c++17 -O3 -o activity1_O3 activity1_pipeline.cpp && ./activity1_O3
```

Fill in your results. If a level is noisy, run it 3-5 times and record the
minimum, not the first number you see:

| Level | One chain | Four chains | Speedup |
|---|---|---|---|
| `-O0` | | | |
| `-O1` | | | |
| `-O2` | | | |
| `-O3` | | | |

## Part 3 — Compare the generated code across `-O1`, `-O2`, `-O3`

Dump the assembly for your file at each level:

```bash
clang++ -std=c++17 -O1 -S -o activity1_O1.s activity1_pipeline.cpp
clang++ -std=c++17 -O2 -S -o activity1_O2.s activity1_pipeline.cpp
clang++ -std=c++17 -O3 -S -o activity1_O3.s activity1_pipeline.cpp
```

Each file contains a labeled block of instructions for `noTempVars` and
another for `withTempVars` (search for `noTempVars`/`withTempVars` in the
label names — C++ name-mangles them, so the label won't be the bare
function name). For each function, compare its instructions across the
three files and answer:

- **(a):** Does `noTempVars`'s generated code change across `-O1`, `-O2`,
  `-O3`?

  ==---==

- **(b):** Does `withTempVars`'s?

  ==---==

- **(c):** Does either use vector/SIMD registers at any level? (`v0`-`v31`
  or `q0`-`q31`; scalar code uses only `x`/`w`.)

  ==---==

- **(d):** Find the four `mul` instructions in each loop body and trace
  their register operands. In `noTempVars` function, does each `mul` read a
  register the `mul` above it just wrote? What about for `withTempVars`? 
  Now count the total instructions in each loop body 
  — what is the count, and how does that square with the speedup 
  you measured in Part 2?

  ==---==

- **(e):** Multiply is associative, so the compiler is *allowed*
  to split `noTempVars`'s chain for you. Your Part 2 numbers show whether it did.
  Did it? What does that tell you about relying on the optimizer for this
  class of transformation?

  ==---==

Back each answer with the specific evidence you found (an instruction
count, the presence or absence of a vector register, a `diff` between two
of the three files) — not just your best guess.

## Part 4 - AI Use Disclosure

State whether you used AI tools for this assignment. If you did, name the tools and briefly describe how you used them. Full credit is earned at every level of use, the course was designed for use with AI as a partner in mind.

==---==

## What to submit

1. Your completed `activity1_pipeline.cpp`. (47.5 points)
2. A document with your completed written answers.
2a. The filled-in results table from Part 2 (raw output is fine, screenshots or copy-pasted terminal text both work). (12.5 points)
2b. Your Part 3 answers (a-e), with the evidence for each. (35 points)
2c. The AI use disclosure. (5 points)


## RB Questions/Reflection
Parting thoughts that are helpful to think about. These are not required for the assignment. 
- Explain how the first version has a RAW dependency?
- Which version of the computation does more instructions?
- What can you conclude about the relationship between the number of instructions and performance?
