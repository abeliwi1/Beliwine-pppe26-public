/*
 * pipeline.dce.cpp
 *
 * Small examples of Dead Code Elimination (DCE) — the compiler pass that
 * removes instructions whose results are never observable.  DCE prevents
 * wasted work from ever entering the pipeline.
 *
 * Three patterns shown:
 *   1. Dead store      — value written but overwritten before any read
 *   2. Dead branch     — condition is compile-time constant; one arm is unreachable
 *   3. Dead pure call  — return value of a side-effect-free function is discarded
 *
 * Compile and benchmark:
 *   clang++ -O2 -o pipeline.dce pipeline.dce.cpp && ./pipeline.dce
 */

#include <chrono>
#include <cstdio>

using namespace std;
using namespace std::chrono;

// Compiler barrier. This only fences the timed region -- it does NOT stop
// the compiler from optimizing inside the functions under test, which is
// the whole point of this file. (An earlier version sprinkled
// doNotOptimize() fences through the loop bodies to keep the "before"
// variants from being optimized away. That guaranteed a dramatic
// before/after gap and taught nothing about what the compiler actually
// does with dead code, so it's gone.)
static void clobber() { asm volatile("" ::: "memory"); }

// Min of `runs`, accumulating every result so none of the calls is dead.
// (`result = fn(arg)` in a loop would let the compiler delete all but the
// last call, and the minimum would come from a run that never happened.)
static double benchMs(long long (*fn)(int), int arg, int runs = 5)
{
    double best = 1e300;
    long long acc = 0;

    for (int r = 0; r < runs; r++) {
        clobber();
        auto t0 = steady_clock::now();
        clobber();

        acc += fn(arg);

        clobber();
        auto t1 = steady_clock::now();
        clobber();

        double ms = duration<double, milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }

    volatile long long sink = acc;
    (void)sink;
    return best;
}

static const int REPS = 100'000'000;

// ============================================================
// 1. Dead Store
//    x is assigned three times; only the last write is ever read.
//    With DCE the first two assignments — and any instructions
//    that feed them — never reach the execution units.
// ============================================================

long long deadStoreBefore(int n)
{
    long long x = 0;
    for (int i = 0; i < n; i++) {
        x = i * i;              // dead: overwritten next line before read
        x = i * i * i;         // dead: overwritten on the line below
        x = i + 1;             // live: this is the value actually accumulated
    }
    return x;
}

long long deadStoreAfter(int n)
{
    long long x = 0;
    for (int i = 0; i < n; i++) {
        // First two assignments removed — only the live store remains.
        x = i + 1;
    }
    return x;
}

// ============================================================
// 2. Dead Branch
//    A compile-time-false condition makes one arm permanently
//    unreachable.  DCE prunes the entire block; no branch
//    instruction, no misprediction, no wasted pipeline slots.
//
//    Note: both before and after measure ~0 ms because the
//    compiler already applies DCE to constexpr-false branches
//    automatically — the after version just makes the intent
//    explicit in source.
// ============================================================

static constexpr bool FEATURE_ENABLED = false;  // resolved at compile time

long long deadBranchBefore(int n)
{
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
        if (FEATURE_ENABLED) {          // always false — dead branch
            sum *= i;                   // dead: block never entered
            sum ^= (sum >> 3);          // dead
        }
    }
    return sum;
}

long long deadBranchAfter(int n)
{
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        sum += i;
        // Dead block removed entirely — loop body is now a single ADD.
    }
    return sum;
}

// ============================================================
// 3. Dead Pure Call
//    expensivePure() has no side effects.  When its return value
//    is discarded the compiler may eliminate the call entirely,
//    saving every cycle the function would have consumed.
// ============================================================

__attribute__((noinline))
static long long expensivePure(long long v)
{
    // Simulate a non-trivial pure computation.
    for (int i = 0; i < 64; i++) v = (v ^ (v >> 7)) * 0x9e3779b97f4a7c15LL;
    return v;
}

long long deadCallBefore(int n)
{
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        expensivePure(i);    // return value discarded — dead call
        sum += i;
    }
    return sum;
}

long long deadCallAfter(int n)
{
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        // Dead call removed — only the accumulation remains.
        sum += i;
    }
    return sum;
}

// ============================================================
// main
// ============================================================

int main()
{
    printf("=== Dead Code Elimination (DCE) ===\n\n");

    printf("[ 1 ] Dead store  (two overwrites before the live write)\n");
    printf("    BEFORE DCE : %6.2f ms\n",   benchMs(deadStoreBefore,  REPS));
    printf("    AFTER  DCE : %6.2f ms\n\n", benchMs(deadStoreAfter,   REPS));

    printf("[ 2 ] Dead branch (compile-time-false condition)\n");
    printf("    BEFORE DCE : %6.2f ms\n",   benchMs(deadBranchBefore, REPS));
    printf("    AFTER  DCE : %6.2f ms\n\n", benchMs(deadBranchAfter,  REPS));

    printf("[ 3 ] Dead pure call (return value discarded)\n");
    printf("    BEFORE DCE : %6.2f ms\n",   benchMs(deadCallBefore,   REPS));
    printf("    AFTER  DCE : %6.2f ms\n\n", benchMs(deadCallAfter,    REPS));

    printf("Key takeaway:\n");
    printf("  Dead code never enters the pipeline — no fetch, no decode,\n");
    printf("  no execution, no stall.  DCE is free performance.\n");

    return 0;
}
