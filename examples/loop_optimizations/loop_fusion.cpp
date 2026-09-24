// Loop Fusion Example
// Two separate loops vs one fused loop to compute mean and variance.
//
// Unfused — two passes:
//   Pass 1: sum all elements -> mean = sum / n
//   Pass 2: sum squared deviations (x - mean)^2 -> variance
//   Pass 2 cannot be merged with Pass 1 naively because it requires
//   mean, which is only known after Pass 1 completes.
//
// Fused — one pass (computational formula):
//   Accumulate sum and sum-of-squares simultaneously in one loop.
//   After the loop:
//     mean     = sum / n
//     variance = (sum_sq / n) - mean * mean
//   Both statistics fall out of a single read of the data.
//
// Build:
//   g++ -O1 -o fusion_O1 loop_fusion.cpp && ./fusion_O1
//   g++ -O2 -o fusion_O2 loop_fusion.cpp && ./fusion_O2

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

static const int N    = 50'000'000;
static const int RUNS = 5;

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

void report(const char* title,
            const char* a_label, double a_ms,
            const char* b_label, double b_ms) {
    double speedup = a_ms / b_ms;
    cout << "\n" << string(64, '-') << "\n";
    cout << title << "\n";
    cout << string(64, '-') << "\n";
    cout << "  UNFUSED  " << left  << setw(44) << a_label
         << right << setw(8) << fixed << setprecision(1) << a_ms << " ms\n";
    cout << "  FUSED    " << left  << setw(44) << b_label
         << right << setw(8) << fixed << setprecision(1) << b_ms  << " ms\n";
    cout << "  Speedup: " << fixed << setprecision(2) << speedup << "x\n";
}

// ================================================================
// Unfused — two passes
// ================================================================

struct Stats { double mean; double variance; };

Stats compute_unfused(const int* data, int n) {
    // Pass 1: mean
    double sum = 0.0;
    for (int i = 0; i < n; i++)
        sum += data[i];
    double mean = sum / n;

    // Pass 2: variance — must wait for mean; reads all N elements again
    double m2 = 0.0;
    for (int i = 0; i < n; i++) {
        double d = data[i] - mean;
        m2 += d * d;
    }
    return {mean, m2 / n};
}

// ================================================================
// Fused — one pass (computational formula)
//
// Uses the identity:  Var(X) = E[X^2] - E[X]^2
//
// Accumulate sum and sum-of-squares in a single loop; derive both
// mean and variance after the loop with no extra data reads.
// ================================================================

Stats compute_fused(const int* data, int n) {
    double sum    = 0.0;
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        double x = data[i];
        sum    += x;
        sum_sq += x * x;
    }
    double mean     = sum / n;
    double variance = (sum_sq / n) - mean * mean;
    return {mean, variance};
}

// ================================================================
// Fused — one pass (Welford's online algorithm)
//
// The textbook numerically-stable single-pass method.  Deviations are taken
// against the running mean rather than a fixed estimate, so it avoids the
// cancellation the computational formula risks.  It reads the array once,
// like the fused version, but pays a division and a loop-carried dependency
// on every element -- which is the point of measuring it here.
// ================================================================

Stats compute_fused_welford(const int* data, int n) {
    double mean = 0.0;
    double m2   = 0.0;
    for (int i = 0; i < n; i++) {
        double delta = data[i] - mean;
        mean += delta / (i + 1);
        m2   += delta * (data[i] - mean);
    }
    return {mean, m2 / n};
}

// ================================================================
// main
// ================================================================

int main() {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();
    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";

    vector<int> data(N);
    for (int i = 0; i < N; i++)
        data[i] = (i * 2654435761u) >> 16 & 0xFFFF;   // pseudo-random 0-65535

    const int* p = data.data();

    auto r1 = compute_unfused(p, N);
    auto r2 = compute_fused  (p, N);

    cout << fixed << setprecision(6);
    cout << "Correctness check:\n";
    cout << "  mean     unfused=" << r1.mean     << "  fused=" << r2.mean     << "\n";
    cout << "  variance unfused=" << r1.variance << "  fused=" << r2.variance << "\n";
    cout << "  delta mean:     " << abs(r1.mean     - r2.mean)     << "\n";
    cout << "  delta variance: " << abs(r1.variance - r2.variance) << "\n";

    double acc = 0.0;
    auto t_unfused = bench([&]{
        auto r = compute_unfused(p, N);
        return r.mean + r.variance;
    }, acc);
    auto t_fused = bench([&]{
        auto r = compute_fused(p, N);
        return r.mean + r.variance;
    }, acc);

    auto t_welford = bench([&]{
        auto r = compute_fused_welford(p, N);
        return r.mean + r.variance;
    }, acc);

    report("mean + variance  (N = 50M)",
           "2 loops — 2 full reads (pass 2 needs mean)",  t_unfused,
           "1 loop  — sum + sum_sq, derive post-loop",     t_fused);

    // Keep every accumulated kernel result observable, or the compiler is
    // entitled to delete the calls and report a time for work that never ran.
    cout << "\n  WELFORD  1 loop  — numerically stable, divide per element "
         << right << setw(8) << fixed << setprecision(1) << t_welford << " ms"
         << "   " << fixed << setprecision(2) << t_unfused / t_welford << "x\n";

    cout << "\nThe array is " << (size_t)N*8/1024/1024 << " MB, far past L3, so both\n"
            "versions are limited by DRAM bandwidth rather than by arithmetic.  That is\n"
            "exactly the regime fusion is for: the unfused version reads the array\n"
            "twice and the fused version reads it once, so the speedup should approach\n"
            "2.0x and does.  Unlike the other examples here, warming the clock barely\n"
            "moves these numbers -- a memory-bound loop runs at the speed of the\n"
            "memory system, not the core, which is a useful way to tell which one you\n"
            "are looking at.\n";

    cout << "\nCore clock at end of run: " << fixed << setprecision(2)
         << core_clock_ghz() << " GHz\n";

    volatile double keep = acc; (void)keep;

    return 0;
}
