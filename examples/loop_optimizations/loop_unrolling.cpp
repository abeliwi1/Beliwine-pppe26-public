// Loop Unrolling Example
// Manual unrolling of a data-driven loop over an array of unknown length,
// and the "tail" problem -- the leftover elements when n is not a multiple
// of the unroll factor.
//
// Four versions are measured:
//   1. Scalar baseline
//   2. 4x unroll with a scalar cleanup loop for the tail
//   3. Duff's Device -- a switch/while hybrid that jumps into the middle of
//      the unrolled body to handle the tail without a separate loop
//   4. 4x unroll with FOUR INDEPENDENT accumulators
//
// THE HEADLINE RESULT ON THIS MACHINE IS THAT (2) AND (3) DO NOTHING.
// Measured across every working set from 8 KB to 64 MB:
//
//     4x unroll        1.01 - 1.04x    (i.e. nothing)
//     Duff's Device    0.94 - 0.97x    (i.e. slightly worse)
//     4x unroll + 4 accumulators       1.64 - 1.65x
//
// This is not a measurement artifact and not a question of array size; the
// ratios are flat across five working sets spanning four orders of
// magnitude.  It is what loop unrolling is actually worth on an
// out-of-order core, and the reason is visible in the cycle count.
//
// The scalar loop runs at 1.02 cycles per element.  Its cost is set by the
// dependent chain through the accumulator `s`: each `s += data[i]` must wait
// for the previous one, and an integer add has 1-cycle latency, so one
// element per cycle is the floor.  Meanwhile the loop control -- increment,
// compare, branch -- issues on other ports, in the same cycle, for free, and
// the branch predictor gets the back-edge right every time.  Unrolling
// removes instructions that were costing nothing.  There is no time in the
// loop that is not the dependency chain, so there is nothing to recover.
//
// Version (4) attacks the chain instead of the loop control: four separate
// accumulators are four independent chains, so four adds are in flight at
// once.  That is worth 1.67x, and it is a different optimisation with a
// different name (see ../ILP/ for multiple accumulators in their own right).
// Unrolling is what MAKES ROOM for it -- you cannot use four accumulators
// without an unrolled body -- which is the honest case for the transform:
// unrolling is an enabler, rarely a win on its own.
//
// Duff's Device loses ~5% because the switch-based entry lands the CPU in
// the middle of a loop body through an indirect jump, which the predictor
// handles worse than the plain back-edge of a normal loop.  On a PDP-11 in
// 1983 it was a real win.  It has not been one for a long time.
//
// Build:
//   g++ -O1 -o unroll_O1 loop_unrolling.cpp && ./unroll_O1
//   g++ -O2 -o unroll_O2 loop_unrolling.cpp && ./unroll_O2
//
// -O1 enables register allocation and basic optimisations but not
// auto-vectorisation.  Build at -O2 and the same sweep reports:
//
//     4x unroll        2.02 - 2.04x
//     Duff's Device    0.93 - 0.94x   (still slower)
//     4x unroll + 4    2.02 - 2.07x
//
// which looks like unrolling finally paying off, and is really the same
// lesson again.  Checked with -fopt-info-vec, GCC vectorises exactly two of
// the four kernels: sum_unrolled4 and sum_unrolled4_acc.  It does NOT
// vectorise sum_scalar, and it does not vectorise Duff's Device at all.  The
// unrolled body is what gives the vectoriser four independent loads to pack;
// the scalar loop presents it with one element at a time and it declines.
// So at -O2 unrolling is again an ENABLER -- this time for the compiler
// rather than for you -- and the two hand-unrolled versions converge, because
// once the vectoriser has the body it does not care how you arranged the
// accumulators.

#include <iostream>
#include <chrono>
#include <climits>
#include <iomanip>
#include <string>
#include <vector>
#include <sched.h>
#include <cstdlib>

using namespace std;
using namespace std::chrono;

// Working sets chosen to sit in L1 (48 KB), L2 (1 MB), L3 (16 MB) and DRAM,
// so the reader can see that the answer does not depend on where the data
// lives.  Repetition counts equalise total element count across cases.
static const int RUNS    = 5;

// Results-header label.  There is no predefined macro for the optimisation
// level (__OPTIMIZE__ is set for -O1 and up without distinguishing them), so
// report only what is detectable; pass -DBUILD_LABEL='"-O2"' to be specific.
#ifndef BUILD_LABEL
#  ifdef __FAST_MATH__
#    define BUILD_LABEL "-ffast-math"
#  elif defined(__OPTIMIZE__)
#    define BUILD_LABEL "optimized"
#  else
#    define BUILD_LABEL "-O0"
#  endif
#endif

// ---------------------------------------------------------------------------
// MACHINE  (AMD Ryzen AI 9 HX 370, "Strix Point", Zen 5 -- Linux, g++ 13.3)
//
// Every figure below was measured on this box, not looked up:
//   L1d          48 KB per core, 12-way, 64 sets, 64-byte lines
//   L2           1 MB per core
//   L3           16 MB shared by the 4 Zen 5 cores (a separate 8 MB serves
//                the 8 Zen 5c cores)
//   core clock   5.13 GHz on cpu0-3, 3.17 GHz on cpu4-11
//
// The 12-way figure comes from a pointer chase over W lines that all map to
// one set (stride 4096 B = one full set-cycle): latency is flat at 1.12 ns
// through W=12 and jumps 3.5x to 3.9 ns at W=13.  The 64-byte line comes from
// a stride sweep -- cost per access grows linearly to stride 64 and is flat
// from 64 to 256, i.e. one line is fetched per access beyond 64 bytes.
//
// TWO TRAPS on this machine, both of which silently corrupt every number in
// this file if you skip them:
//
//  1. HETEROGENEOUS CORES.  cpu0-3 are Zen 5 cores at 5.13 GHz; cpu4-11 are
//     Zen 5c "dense" cores at 3.17 GHz.  An unpinned run lands wherever the
//     scheduler puts it, so the same binary reports times differing by 1.6x
//     for no reason at all.  We pin to cpu0.
//
//  2. CLOCK RAMP.  amd-pstate-epp in its default powersave /
//     balance_performance mode raises the clock in response to high IPC --
//     not to the core merely being busy.  A memory-bound loop has LOW IPC, so
//     the governor never boosts it: the core sits at ~3.59 GHz for the entire
//     run, confirmed against /sys/.../scaling_cur_freq while measuring.  Min-of-RUNS does NOT rescue
//     you -- every run is equally slow.  Measured on the transpose in
//     loop_tiling.cpp: 263 ms on all 8 repetitions cold, 184 ms on all 8
//     after a warm-up spin, a 1.43x error that looks perfectly stable.
//     So: spin on a high-IPC loop first, and print the clock so you can see
//     what you actually measured at.
// ---------------------------------------------------------------------------

// cpu0 by default; override with PIN_CPU=n to move off a busy core.  Keep it
// on one of the fast cores (cpu0-3 here) or the times are not comparable.
static int pin_to_fast_core(int cpu = 0) {
    if (const char* e = getenv("PIN_CPU")) cpu = atoi(e);
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        cerr << "warning: could not pin to cpu" << cpu << "\n";
    return cpu;
}

// A dependent chain of integer ADDs retires one per cycle, so this reports the
// clock the core is running at *right now* -- not its nameplate maximum.
// Unrolled 8x because at one ADD per iteration the loop control, not the
// chain, sets the rate.  Kept short (~20 ms) so that the probe itself, which
// is low-IPC, does not talk the governor back down while it runs.
static double core_clock_ghz() {
    const long n = 100'000'000;
    long a = 0;
    auto t0 = steady_clock::now();
    for (long i = 0; i < n / 8; i++)
        asm volatile("addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\t"
                     "addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0"
                     : "+r"(a));
    auto t1 = steady_clock::now();
    return n / duration<double>(t1 - t0).count() / 1e9;
}

// Raise the clock before measuring anything, and report where it landed.
//
// The governor responds to INSTRUCTIONS PER CYCLE, not to the core merely
// being busy -- which is a sharper trap than it sounds.  Spinning on the
// dependent chain above does NOT boost the core: eight serialised ADDs plus
// loop control is 10 instructions per 8 cycles, about 1.25 IPC, and the core
// sits at 3.57 GHz indefinitely (measured: flat over 12 consecutive calls).
// Eight INDEPENDENT chains retire ~8 instructions per cycle and take the core
// to 5.13 GHz in well under a second, where it then stays.
static double warm_up(double seconds = 0.8) {
    long a0=0,a1=0,a2=0,a3=0,a4=0,a5=0,a6=0,a7=0;
    auto t0 = steady_clock::now();
    while (duration<double>(steady_clock::now() - t0).count() < seconds)
        for (int i = 0; i < 200000; i++)
            asm volatile("addq $1,%0\n\taddq $1,%1\n\taddq $1,%2\n\taddq $1,%3\n\t"
                         "addq $1,%4\n\taddq $1,%5\n\taddq $1,%6\n\taddq $1,%7"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),
                           "+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
    return core_clock_ghz();
}

// Compiler barrier.  Fences the timed region only -- it says nothing about the
// code inside the kernels, which is the whole point.
static void clobber() { asm volatile("" ::: "memory"); }

// Min-of-RUNS wall time in milliseconds, at sub-millisecond resolution.
//
// The lambda returns its kernel's result and every one is accumulated into
// `acc`, so no call is ever dead.  An earlier version timed in whole
// milliseconds, which rounded real differences away -- effects under ~1 ms
// showed up as ties or as noise-sized "speedups".
template <typename Func>
double bench(Func f, double& acc) {
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        clobber();
        auto t0 = high_resolution_clock::now();
        clobber();

        acc += (double)f();

        clobber();
        auto t1 = high_resolution_clock::now();
        clobber();

        double ms = duration<double, milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

// ================================================================
// 1. Scalar baseline — one add per iteration
// ================================================================

long long sum_scalar(const int* data, int n) {
    long long s = 0;
    for (int i = 0; i < n; i++)
        s += data[i];
    return s;
}

// ================================================================
// 2. 4x unroll with a scalar cleanup loop
//
// Main loop processes 4 elements at once, reducing loop-control
// overhead (branch, increment) by 4x.  A trailing scalar loop
// handles the remaining 0–3 elements.
// ================================================================

long long sum_unrolled4(const int* data, int n) {
    long long s = 0;
    int i = 0;

    // Process 4 elements per iteration.
    int n4 = n - (n % 4);   // largest multiple of 4 <= n
    for (; i < n4; i += 4) {
        s += data[i];
        s += data[i + 1];
        s += data[i + 2];
        s += data[i + 3];
    }

    // Cleanup: handle tail elements (0, 1, 2, or 3 remaining).
    for (; i < n; i++)
        s += data[i];

    return s;
}

// ================================================================
// 3. Duff's Device — unrolled body with a switch-driven entry point
//
// The switch jumps into the *middle* of the unrolled loop body so
// that the first "iteration" processes exactly (n % 4) elements,
// aligning subsequent iterations to a 4-element boundary.
// No separate cleanup loop is required.
//
// Named after Tom Duff (Bell Labs, 1983) who used it to accelerate
// a byte-copy routine on a PDP-11.
// ================================================================

long long sum_duffs(const int* data, int n) {
    if (n == 0) return 0;

    long long s = 0;
    int i = 0;
    int count = (n + 3) / 4;   // number of times the unrolled body fires

    // switch selects the fall-through entry point for the first pass.
    switch (n % 4) {
        case 0: do { s += data[i++];  // falls through
        case 3:      s += data[i++];  // falls through
        case 2:      s += data[i++];  // falls through
        case 1:      s += data[i++];
                } while (--count > 0);
    }

    return s;
}

// ================================================================
// 4. 4x unroll with four INDEPENDENT accumulators
//
// Identical memory traffic and identical instruction count to version 2.
// The only change is that the four adds in the body no longer form one
// dependency chain, so the out-of-order engine can have four in flight.
// This is the version that is actually faster.
// ================================================================

long long sum_unrolled4_acc(const int* data, int n) {
    long long s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    int i = 0;

    int n4 = n - (n % 4);
    for (; i < n4; i += 4) {
        s0 += data[i];
        s1 += data[i + 1];
        s2 += data[i + 2];
        s3 += data[i + 3];
    }

    for (; i < n; i++)
        s0 += data[i];

    return s0 + s1 + s2 + s3;
}

// ================================================================
// main — build the array and run benchmarks
// ================================================================

int main() {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();
    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";

    // Each case keeps total elements summed roughly constant, so the times
    // are directly comparable down the column.
    struct Case { const char* where; int n; long long reps; };
    const Case cases[] = {
        { "8 KB   (L1)",       2048, 200000 },
        { "32 KB  (L1)",       8192,  50000 },
        { "256 KB (L2)",      65536,   6000 },
        { "4 MB   (L3)",    1048576,    400 },
        { "64 MB  (DRAM)", 16777216,     25 },
    };

    cout << "\n" << string(78, '-') << "\n";
    cout << "Sum of an int array, odd length (n % 4 == 1, so every version runs its tail)\n";
    cout << string(78, '-') << "\n";
    cout << left  << setw(16) << "working set"
         << right << setw(10) << "scalar"
         << right << setw(10) << "unroll4"
         << right << setw(10) << "duffs"
         << right << setw(11) << "unroll4+4"
         << right << setw(9)  << "cyc/el" << "\n";
    cout << string(78, '-') << "\n";

    double acc = 0.0;
    for (const Case& c : cases) {
        vector<int> data(c.n);
        for (int i = 0; i < c.n; i++)
            data[i] = (i % 7) + 1;      // values 1-7, no power-of-2 pattern
        const int* p = data.data();
        const int  n = c.n - 3;         // remainder 1: exercises every tail path

        // Correctness: all four must agree, including on the tail.
        long long r0 = sum_scalar(p, n),       r1 = sum_unrolled4(p, n);
        long long r2 = sum_duffs(p, n),        r3 = sum_unrolled4_acc(p, n);
        if (!(r0 == r1 && r1 == r2 && r2 == r3)) {
            cout << "FAIL: versions disagree at n=" << n << "\n";
            return 1;
        }

        // The array and the length are the same every repetition, so the call
        // is loop-invariant and the compiler is entitled to hoist it out and
        // run it once.  Perturbing one element per repetition makes the input
        // genuinely different each time.  (Without this the whole sweep
        // reports 0.0 ms, which is how the trap announces itself.)
        int* mut = data.data();
        auto sweep = [&](int which) {
            double best = 1e300;
            for (int r = 0; r < RUNS; r++) {
                clobber();
                auto t0 = high_resolution_clock::now();
                clobber();
                long long a = 0;
                for (long long q = 0; q < c.reps; q++) {
                    mut[0] = (int)(q & 7) + 1;
                    switch (which) {
                        case 0: a += sum_scalar       (p, n); break;
                        case 1: a += sum_unrolled4    (p, n); break;
                        case 2: a += sum_duffs        (p, n); break;
                        default:a += sum_unrolled4_acc(p, n); break;
                    }
                }
                clobber();
                auto t1 = high_resolution_clock::now();
                clobber();
                acc += (double)a;
                double ms = duration<double, milli>(t1 - t0).count();
                if (ms < best) best = ms;
            }
            return best;
        };

        double t0 = sweep(0), t1 = sweep(1), t2 = sweep(2), t3 = sweep(3);
        double elems = (double)n * (double)c.reps;

        cout << left  << setw(16) << c.where
             << right << setw(8)  << fixed << setprecision(1) << t0 << "ms"
             << right << setw(8)  << t1 << "ms"
             << right << setw(8)  << t2 << "ms"
             << right << setw(9)  << t3 << "ms"
             << right << setw(9)  << fixed << setprecision(2)
             << (t0 * 1e-3 * ghz * 1e9) / elems << "\n";
        cout << left  << setw(16) << ""
             << right << setw(10) << "1.00x"
             << right << setw(9)  << fixed << setprecision(2) << t0/t1 << "x"
             << right << setw(9)  << t0/t2 << "x"
             << right << setw(10) << t0/t3 << "x"
             << right << setw(9)  << "" << "\n";
    }

    cout << "\nWhat this shows:\n"
            "\n"
            "  The 'cyc/el' column is the scalar loop's cost per element, and it is\n"
            "  1.0 at every working set -- even at 64 MB, where the data comes from\n"
            "  DRAM.  One element per cycle is exactly the latency of the dependent\n"
            "  chain through the accumulator, so the loop is chain-bound everywhere;\n"
            "  the memory system keeps up with it without difficulty at every level.\n"
            "\n"
            "  Removing loop control (unroll4) therefore recovers nothing: those\n"
            "  instructions were issuing in parallel with the chain and costing zero\n"
            "  cycles.  You cannot speed up a loop by deleting work that was free.\n"
            "\n"
            "  Duff's Device is consistently a few percent SLOWER.  The indirect jump\n"
            "  into the middle of the body predicts worse than an ordinary back-edge.\n"
            "\n"
            "  Breaking the chain (unroll4+4 accumulators) is worth ~1.67x, because\n"
            "  it attacks the thing that actually costs time.  Unrolling's real value\n"
            "  is that it creates the space to do this -- not the instruction count.\n"
            "\n"
            "  Build at -O2 and the unrolled versions jump to ~2.0x -- but only\n"
            "  because GCC vectorises them.  It does not vectorise the scalar loop\n"
            "  (checked with -fopt-info-vec), so unrolling is again the enabler and\n"
            "  not the win.  Duff's Device is not vectorised either, and stays slow.\n";

    cout << "\nCore clock at end of run: " << fixed << setprecision(2)
         << core_clock_ghz() << " GHz\n";

    // Keep every accumulated kernel result observable, or the compiler is
    // entitled to delete the calls and report a time for work that never ran.
    volatile double keep = acc; (void)keep;

    return 0;
}
