// Loop Fission Example
// Splitting a high-register-pressure loop into several lower-pressure loops
// to eliminate compiler spills to the stack.
//
// Problem: compute 32 independent dot products (correlations) of one input
// signal against 32 reference vectors, all in a single loop.  The loop body
// needs 32 accumulators + 32 reference pointers + signal pointer + loop
// counter + current sample alive at once.  x86-64 has 16 architectural XMM
// registers and 16 general-purpose ones, so this is roughly a 4x oversubscribed
// register file; the compiler spills accumulators to the stack and adds a
// load and a store per accumulator per iteration.
//
// Fission splits that one loop into several, each handling a slice of the
// correlations, so that a slice fits in the register file.  The cost is that
// the signal is re-read once per slice -- so there is an optimum, and it is
// not simply "as small as possible".
//
// HOW MANY WAYS TO SPLIT?  Measured on this machine (32 correlations,
// 10M samples, g++ -O1).  The last column is the number of instructions that
// touch the stack inside the function, straight out of the generated assembly:
//
//     accums   loops     time   speedup   stack refs
//         32       1   237.9ms     1.00x          134
//         16       2    92.0ms     2.59x           57
//          8       4    55.6ms     4.28x            0   <- spills hit zero
//          4       8    53.1ms     4.48x            0
//
// The speedup tracks the spill count exactly, which is the point: this is a
// register-allocation effect and the assembly says so out loud.  The knee is
// at 8 accumulators per loop, where the spills disappear entirely.  Splitting
// further to 4 buys nothing measurable -- there were no spills left to remove,
// and the eight extra passes over the signal cancel what little is gained.
//
// Note that 16 accumulators is NOT enough on this target.  x86-64 has 16
// architectural XMM registers, and 16 accumulators plus the reference pointers
// and the loop state still overflow them: 57 stack references remain.  On an
// ISA with 32 FP registers, 16 per loop would fit.  The register count is an
// ISA property, so the best fission factor is not portable -- re-measure it
// whenever you change targets.
//
// To see the spill/reload instructions:
//   g++ -O1 -S -o fission.s loop_fission.cpp
//   grep -c '(%rsp)' fission.s          # stack traffic in the inner loops
//
// Build:
//   g++ -O1 -o fission_O1 loop_fission.cpp && ./fission_O1

#include <iostream>
#include <chrono>
#include <climits>
#include <cmath>
#include <iomanip>
#include <string>
#include <vector>
#include <sched.h>
#include <cstdlib>

using namespace std;
using namespace std::chrono;

static const int N       = 10'000'000;   // samples per signal
static const int REFS    = 32;           // number of reference vectors
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
// Unfissioned — all 32 correlations in one loop
//
// Live inside the loop body:
//   32 accumulators (c[0..31])
//   32 reference pointers (ref[0..31])
//    1 signal pointer
//    1 loop counter
//    1 current sample (s)
//   --------------------------------
//   ~67 values competing for 16 XMM + 16 GP registers (x86-64)
//   -> compiler must spill accumulators to the stack
// ================================================================

void correlate_fused(const float* signal,
                    const float* const* ref,
                    float* c, int n) {
    // correlations 0-31
    float a0 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float a4 = 0.0f;
    float a5 = 0.0f;
    float a6 = 0.0f;
    float a7 = 0.0f;
    float a8 = 0.0f;
    float a9 = 0.0f;
    float a10 = 0.0f;
    float a11 = 0.0f;
    float a12 = 0.0f;
    float a13 = 0.0f;
    float a14 = 0.0f;
    float a15 = 0.0f;
    float a16 = 0.0f;
    float a17 = 0.0f;
    float a18 = 0.0f;
    float a19 = 0.0f;
    float a20 = 0.0f;
    float a21 = 0.0f;
    float a22 = 0.0f;
    float a23 = 0.0f;
    float a24 = 0.0f;
    float a25 = 0.0f;
    float a26 = 0.0f;
    float a27 = 0.0f;
    float a28 = 0.0f;
    float a29 = 0.0f;
    float a30 = 0.0f;
    float a31 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a0  += s * ref[ 0][i];
        a1  += s * ref[ 1][i];
        a2  += s * ref[ 2][i];
        a3  += s * ref[ 3][i];
        a4  += s * ref[ 4][i];
        a5  += s * ref[ 5][i];
        a6  += s * ref[ 6][i];
        a7  += s * ref[ 7][i];
        a8  += s * ref[ 8][i];
        a9  += s * ref[ 9][i];
        a10 += s * ref[10][i];
        a11 += s * ref[11][i];
        a12 += s * ref[12][i];
        a13 += s * ref[13][i];
        a14 += s * ref[14][i];
        a15 += s * ref[15][i];
        a16 += s * ref[16][i];
        a17 += s * ref[17][i];
        a18 += s * ref[18][i];
        a19 += s * ref[19][i];
        a20 += s * ref[20][i];
        a21 += s * ref[21][i];
        a22 += s * ref[22][i];
        a23 += s * ref[23][i];
        a24 += s * ref[24][i];
        a25 += s * ref[25][i];
        a26 += s * ref[26][i];
        a27 += s * ref[27][i];
        a28 += s * ref[28][i];
        a29 += s * ref[29][i];
        a30 += s * ref[30][i];
        a31 += s * ref[31][i];
    }
    c[ 0] = a0;
    c[ 1] = a1;
    c[ 2] = a2;
    c[ 3] = a3;
    c[ 4] = a4;
    c[ 5] = a5;
    c[ 6] = a6;
    c[ 7] = a7;
    c[ 8] = a8;
    c[ 9] = a9;
    c[10] = a10;
    c[11] = a11;
    c[12] = a12;
    c[13] = a13;
    c[14] = a14;
    c[15] = a15;
    c[16] = a16;
    c[17] = a17;
    c[18] = a18;
    c[19] = a19;
    c[20] = a20;
    c[21] = a21;
    c[22] = a22;
    c[23] = a23;
    c[24] = a24;
    c[25] = a25;
    c[26] = a26;
    c[27] = a27;
    c[28] = a28;
    c[29] = a29;
    c[30] = a30;
    c[31] = a31;

}

// ================================================================
// Fissioned -- the same 32 correlations, split S ways
//
// Live inside each loop body, for a slice of width C = 32/S:
//   C accumulators + C reference pointers + signal pointer + counter + sample
//
// C=16 still oversubscribes 16 XMM registers.  C=8 fits.  C=4 fits with room
// to spare but pays for eight passes over the signal instead of four.
// ================================================================

void correlate_split16(const float* signal,
                      const float* const* ref,
                      float* c, int n) {
    // correlations 0-15
    float a0 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float a4 = 0.0f;
    float a5 = 0.0f;
    float a6 = 0.0f;
    float a7 = 0.0f;
    float a8 = 0.0f;
    float a9 = 0.0f;
    float a10 = 0.0f;
    float a11 = 0.0f;
    float a12 = 0.0f;
    float a13 = 0.0f;
    float a14 = 0.0f;
    float a15 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a0  += s * ref[ 0][i];
        a1  += s * ref[ 1][i];
        a2  += s * ref[ 2][i];
        a3  += s * ref[ 3][i];
        a4  += s * ref[ 4][i];
        a5  += s * ref[ 5][i];
        a6  += s * ref[ 6][i];
        a7  += s * ref[ 7][i];
        a8  += s * ref[ 8][i];
        a9  += s * ref[ 9][i];
        a10 += s * ref[10][i];
        a11 += s * ref[11][i];
        a12 += s * ref[12][i];
        a13 += s * ref[13][i];
        a14 += s * ref[14][i];
        a15 += s * ref[15][i];
    }
    c[ 0] = a0;
    c[ 1] = a1;
    c[ 2] = a2;
    c[ 3] = a3;
    c[ 4] = a4;
    c[ 5] = a5;
    c[ 6] = a6;
    c[ 7] = a7;
    c[ 8] = a8;
    c[ 9] = a9;
    c[10] = a10;
    c[11] = a11;
    c[12] = a12;
    c[13] = a13;
    c[14] = a14;
    c[15] = a15;

    // correlations 16-31
    float a16 = 0.0f;
    float a17 = 0.0f;
    float a18 = 0.0f;
    float a19 = 0.0f;
    float a20 = 0.0f;
    float a21 = 0.0f;
    float a22 = 0.0f;
    float a23 = 0.0f;
    float a24 = 0.0f;
    float a25 = 0.0f;
    float a26 = 0.0f;
    float a27 = 0.0f;
    float a28 = 0.0f;
    float a29 = 0.0f;
    float a30 = 0.0f;
    float a31 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a16 += s * ref[16][i];
        a17 += s * ref[17][i];
        a18 += s * ref[18][i];
        a19 += s * ref[19][i];
        a20 += s * ref[20][i];
        a21 += s * ref[21][i];
        a22 += s * ref[22][i];
        a23 += s * ref[23][i];
        a24 += s * ref[24][i];
        a25 += s * ref[25][i];
        a26 += s * ref[26][i];
        a27 += s * ref[27][i];
        a28 += s * ref[28][i];
        a29 += s * ref[29][i];
        a30 += s * ref[30][i];
        a31 += s * ref[31][i];
    }
    c[16] = a16;
    c[17] = a17;
    c[18] = a18;
    c[19] = a19;
    c[20] = a20;
    c[21] = a21;
    c[22] = a22;
    c[23] = a23;
    c[24] = a24;
    c[25] = a25;
    c[26] = a26;
    c[27] = a27;
    c[28] = a28;
    c[29] = a29;
    c[30] = a30;
    c[31] = a31;

}

void correlate_split8(const float* signal,
                     const float* const* ref,
                     float* c, int n) {
    // correlations 0-7
    float a0 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float a4 = 0.0f;
    float a5 = 0.0f;
    float a6 = 0.0f;
    float a7 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a0  += s * ref[ 0][i];
        a1  += s * ref[ 1][i];
        a2  += s * ref[ 2][i];
        a3  += s * ref[ 3][i];
        a4  += s * ref[ 4][i];
        a5  += s * ref[ 5][i];
        a6  += s * ref[ 6][i];
        a7  += s * ref[ 7][i];
    }
    c[ 0] = a0;
    c[ 1] = a1;
    c[ 2] = a2;
    c[ 3] = a3;
    c[ 4] = a4;
    c[ 5] = a5;
    c[ 6] = a6;
    c[ 7] = a7;

    // correlations 8-15
    float a8 = 0.0f;
    float a9 = 0.0f;
    float a10 = 0.0f;
    float a11 = 0.0f;
    float a12 = 0.0f;
    float a13 = 0.0f;
    float a14 = 0.0f;
    float a15 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a8  += s * ref[ 8][i];
        a9  += s * ref[ 9][i];
        a10 += s * ref[10][i];
        a11 += s * ref[11][i];
        a12 += s * ref[12][i];
        a13 += s * ref[13][i];
        a14 += s * ref[14][i];
        a15 += s * ref[15][i];
    }
    c[ 8] = a8;
    c[ 9] = a9;
    c[10] = a10;
    c[11] = a11;
    c[12] = a12;
    c[13] = a13;
    c[14] = a14;
    c[15] = a15;

    // correlations 16-23
    float a16 = 0.0f;
    float a17 = 0.0f;
    float a18 = 0.0f;
    float a19 = 0.0f;
    float a20 = 0.0f;
    float a21 = 0.0f;
    float a22 = 0.0f;
    float a23 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a16 += s * ref[16][i];
        a17 += s * ref[17][i];
        a18 += s * ref[18][i];
        a19 += s * ref[19][i];
        a20 += s * ref[20][i];
        a21 += s * ref[21][i];
        a22 += s * ref[22][i];
        a23 += s * ref[23][i];
    }
    c[16] = a16;
    c[17] = a17;
    c[18] = a18;
    c[19] = a19;
    c[20] = a20;
    c[21] = a21;
    c[22] = a22;
    c[23] = a23;

    // correlations 24-31
    float a24 = 0.0f;
    float a25 = 0.0f;
    float a26 = 0.0f;
    float a27 = 0.0f;
    float a28 = 0.0f;
    float a29 = 0.0f;
    float a30 = 0.0f;
    float a31 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a24 += s * ref[24][i];
        a25 += s * ref[25][i];
        a26 += s * ref[26][i];
        a27 += s * ref[27][i];
        a28 += s * ref[28][i];
        a29 += s * ref[29][i];
        a30 += s * ref[30][i];
        a31 += s * ref[31][i];
    }
    c[24] = a24;
    c[25] = a25;
    c[26] = a26;
    c[27] = a27;
    c[28] = a28;
    c[29] = a29;
    c[30] = a30;
    c[31] = a31;

}

void correlate_split4(const float* signal,
                     const float* const* ref,
                     float* c, int n) {
    // correlations 0-3
    float a0 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a0  += s * ref[ 0][i];
        a1  += s * ref[ 1][i];
        a2  += s * ref[ 2][i];
        a3  += s * ref[ 3][i];
    }
    c[ 0] = a0;
    c[ 1] = a1;
    c[ 2] = a2;
    c[ 3] = a3;

    // correlations 4-7
    float a4 = 0.0f;
    float a5 = 0.0f;
    float a6 = 0.0f;
    float a7 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a4  += s * ref[ 4][i];
        a5  += s * ref[ 5][i];
        a6  += s * ref[ 6][i];
        a7  += s * ref[ 7][i];
    }
    c[ 4] = a4;
    c[ 5] = a5;
    c[ 6] = a6;
    c[ 7] = a7;

    // correlations 8-11
    float a8 = 0.0f;
    float a9 = 0.0f;
    float a10 = 0.0f;
    float a11 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a8  += s * ref[ 8][i];
        a9  += s * ref[ 9][i];
        a10 += s * ref[10][i];
        a11 += s * ref[11][i];
    }
    c[ 8] = a8;
    c[ 9] = a9;
    c[10] = a10;
    c[11] = a11;

    // correlations 12-15
    float a12 = 0.0f;
    float a13 = 0.0f;
    float a14 = 0.0f;
    float a15 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a12 += s * ref[12][i];
        a13 += s * ref[13][i];
        a14 += s * ref[14][i];
        a15 += s * ref[15][i];
    }
    c[12] = a12;
    c[13] = a13;
    c[14] = a14;
    c[15] = a15;

    // correlations 16-19
    float a16 = 0.0f;
    float a17 = 0.0f;
    float a18 = 0.0f;
    float a19 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a16 += s * ref[16][i];
        a17 += s * ref[17][i];
        a18 += s * ref[18][i];
        a19 += s * ref[19][i];
    }
    c[16] = a16;
    c[17] = a17;
    c[18] = a18;
    c[19] = a19;

    // correlations 20-23
    float a20 = 0.0f;
    float a21 = 0.0f;
    float a22 = 0.0f;
    float a23 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a20 += s * ref[20][i];
        a21 += s * ref[21][i];
        a22 += s * ref[22][i];
        a23 += s * ref[23][i];
    }
    c[20] = a20;
    c[21] = a21;
    c[22] = a22;
    c[23] = a23;

    // correlations 24-27
    float a24 = 0.0f;
    float a25 = 0.0f;
    float a26 = 0.0f;
    float a27 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a24 += s * ref[24][i];
        a25 += s * ref[25][i];
        a26 += s * ref[26][i];
        a27 += s * ref[27][i];
    }
    c[24] = a24;
    c[25] = a25;
    c[26] = a26;
    c[27] = a27;

    // correlations 28-31
    float a28 = 0.0f;
    float a29 = 0.0f;
    float a30 = 0.0f;
    float a31 = 0.0f;
    for (int i = 0; i < n; i++) {
        float s = signal[i];
        a28 += s * ref[28][i];
        a29 += s * ref[29][i];
        a30 += s * ref[30][i];
        a31 += s * ref[31][i];
    }
    c[28] = a28;
    c[29] = a29;
    c[30] = a30;
    c[31] = a31;

}

// ================================================================
// main
// ================================================================

int main() {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();
    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";

    // Build signal and reference vectors with distinct values.
    vector<float> signal(N);
    vector<vector<float>> ref_data(REFS, vector<float>(N));
    for (int i = 0; i < N; i++) signal[i] = sinf(i * 0.001f);
    for (int k = 0; k < REFS; k++)
        for (int i = 0; i < N; i++)
            ref_data[k][i] = sinf(i * 0.001f * (k + 1));

    // Build pointer array for easy passing.
    vector<const float*> refs(REFS);
    for (int k = 0; k < REFS; k++) refs[k] = ref_data[k].data();

    const float* sig = signal.data();
    const float* const* rp = refs.data();

    vector<float> c1(REFS), c2(REFS);

    // Correctness check.
    correlate_fused  (sig, rp, c1.data(), N);
    correlate_split8(sig, rp, c2.data(), N);
    cout << "Correctness check:\n";
    bool ok = true;
    for (int k = 0; k < REFS; k++) {
        float delta = fabsf(c1[k] - c2[k]);
        if (delta > 1e-3f) { ok = false; cout << "  FAIL k=" << k << "\n"; }
    }
    cout << "  " << (ok ? "PASS" : "FAIL") << "\n";

    double acc = 0.0;
    auto t_fused   = bench([&]{ correlate_fused  (sig, rp, c1.data(), N); return c1[0]; }, acc);
    auto t_s16 = bench([&]{ correlate_split16(sig, rp, c2.data(), N); return c2[0]; }, acc);
    auto t_s8  = bench([&]{ correlate_split8 (sig, rp, c2.data(), N); return c2[0]; }, acc);
    auto t_s4  = bench([&]{ correlate_split4 (sig, rp, c2.data(), N); return c2[0]; }, acc);

    cout << "\n" << string(68, '-') << "\n";
    cout << "32 correlations over " << N/1000000 << "M samples"
            "  --  register pressure vs loop fission\n";
    cout << string(68, '-') << "\n";
    cout << left  << setw(34) << "split"
         << right << setw(12) << "accums"
         << right << setw(11) << "time"
         << right << setw(10) << "speedup" << "\n";
    cout << string(68, '-') << "\n";

    struct Row { const char* how; int accums; double ms; };
    const Row rows[] = {
        { "1 loop    (unfissioned)",      32, t_fused },
        { "2 loops",                      16, t_s16   },
        { "4 loops   (spills reach zero)", 8, t_s8    },
        { "8 loops",                       4, t_s4    },
    };
    for (const Row& r : rows)
        cout << left  << setw(34) << r.how
             << right << setw(12) << r.accums
             << right << setw(8)  << fixed << setprecision(1) << r.ms << " ms"
             << right << setw(9)  << fixed << setprecision(2)
             << t_fused / r.ms << "x" << "\n";

    cout << "\nThe knee is at 8 accumulators per loop, where the spills reach zero.\n"
            "Count them yourself -- the speedup tracks the stack traffic exactly:\n"
            "\n"
            "  g++ -O1 -S -o fission.s loop_fission.cpp\n"
            "  for f in fused split16 split8 split4; do \\\n"
            "      echo -n \"$f \"; sed -n \"/correlate_$f/,/^\\s*\\.size/p\" fission.s \\\n"
            "      | grep -c '(%rsp)'; done\n"
            "\n"
            "    32 accumulators -> 134 stack refs      8 accumulators ->   0\n"
            "    16 accumulators ->  57 stack refs      4 accumulators ->   0\n"
            "\n"
            "Sixteen is not enough on this target: x86-64 has 16 architectural XMM\n"
            "registers, and 16 accumulators plus the reference pointers and loop\n"
            "state still overflow them.  On an ISA with 32 FP registers it would\n"
            "fit.  The best split is an ISA property -- re-measure it per target.\n"
            "\nSplitting past the knee buys nothing: at 4 accumulators there are no\n"
            "spills left to remove, and eight passes over the signal cancel the rest.\n"
            "\nOne measurement trap worth knowing: the reference vectors are 32\n"
            "separate 40 MB allocations and malloc hands them back with identical\n"
            "alignment, which makes the streams congruent in the cache and in the\n"
            "DRAM banks.  Staggering the allocations by a cache line each is worth\n"
            "a large fraction of the 1-loop time on its own, and has nothing to do\n"
            "with fission.  The times above use the natural allocation.\n";

    cout << "\nCore clock at end of run: " << fixed << setprecision(2)
         << core_clock_ghz() << " GHz\n";

    // Keep every accumulated kernel result observable, or the compiler is
    // entitled to delete the calls and report a time for work that never ran.
    volatile double keep = acc; (void)keep;

    return 0;
}
