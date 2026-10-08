// SIMD Bitonic Sort for 16 int32 — Google Highway (portable)
//
// Same 10-step comparator network as bitonic_sort_simd.cpp but written with
// Google Highway instead of ARM NEON intrinsics.  Highway selects the best
// available instruction set at compile time (NEON, SSE4, AVX2, AVX-512, ...)
// from a single source file — no architecture-specific #ifdefs required.
//
// Layout: four 128-bit vectors hold the 16 elements:
//   v0 = [a0..a3]   v1 = [a4..a7]
//   v2 = [a8..a11]  v3 = [a12..a15]
//
// Five intra-register CAS helpers replace the NEON vrev64/vext/vbsl sequences:
//
//   cas_asc_adj   — adjacent pairs (0,1)(2,3) both ascending   → [min,max,min,max]
//   cas_desc_adj  — adjacent pairs (0,1)(2,3) both descending  → [max,min,max,min]
//   cas_mixed_adj — lower pair (0,1)↑, upper pair (2,3)↓       → [min,max,max,min]
//   cas_asc_skip2 — stride-2  pairs (0,2)(1,3) ascending       → [min,min,max,max]
//   cas_desc_skip2— stride-2  pairs (0,2)(1,3) descending      → [max,max,min,min]
//
// Cross-register steps (4, 7, 8) use element-wise Min/Max directly.
//
// Key Highway ops used:
//   Reverse2(d, v)              — swap adjacent pairs: [a,b,c,d]→[b,a,d,c]
//   CombineShiftRightBytes<8>   — rotate by 2 lanes:  [a,b,c,d]→[c,d,a,b]
//   OddEven(odd_src, even_src)  — lane interleave (even←even_src, odd←odd_src)
//   LowerHalf / UpperHalf / Combine — split and rejoin 64-bit halves
//
// Build:
//   clang++ -O2 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib \
//     bitonic_sort_highway.cpp -o bitonic_sort_highway -lhwy && ./bitonic_sort_highway

#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <climits>
#include <string>
#include <cmath>
#include "hwy/highway.h"
#include <cstdio>
#include <cstdlib>

#if defined(__linux__)
  #include <sched.h>
#elif defined(__APPLE__)
  #include <pthread.h>
  #include <sys/qos.h>
#endif

using namespace std;
using namespace std::chrono;
namespace hn = hwy::HWY_NAMESPACE;

static const int RUNS = 7;
static const int N    = 16;

// ================================================================
// Type aliases for 4-wide and 2-wide int32 vectors
// ================================================================
using D4 = hn::FixedTag<int32_t, 4>;
using D2 = hn::Half<D4>;              // FixedTag<int32_t, 2>
using V4 = hn::Vec<D4>;
using V2 = hn::Vec<D2>;

// ================================================================
// Intra-register CAS helpers
//
// For v = [a, b, c, d]:
//   Reverse2(d4, v)               → [b, a, d, c]  (swap adjacent pairs)
//   CombineShiftRightBytes<8>(d4,v,v) → [c, d, a, b]  (rotate by 2 elements)
//
//   OddEven(odd_src, even_src): even-indexed lanes ← even_src,
//                               odd-indexed  lanes ← odd_src
//
// All helpers are provably branchless: Min/Max/OddEven/Combine each
// compile to two or three SIMD instructions on any supported target.
// ================================================================

static inline V4 cas_asc_adj(V4 v) {
    const D4 d4;
    V4 rv = hn::Reverse2(d4, v);                 // [b,a,d,c]
    V4 lo = hn::Min(v, rv), hi = hn::Max(v, rv); // both symmetric
    return hn::OddEven(hi, lo);                   // [min,max,min,max]
}

static inline V4 cas_desc_adj(V4 v) {
    const D4 d4;
    V4 rv = hn::Reverse2(d4, v);
    V4 lo = hn::Min(v, rv), hi = hn::Max(v, rv);
    return hn::OddEven(lo, hi);                   // [max,min,max,min]
}

// Lower pair ascending, upper pair descending: [min01, max01, max23, min23]
static inline V4 cas_mixed_adj(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::Reverse2(d4, v);
    V4 lo = hn::Min(v, rv), hi = hn::Max(v, rv);
    V4 asc  = hn::OddEven(hi, lo);               // [min01,max01,min23,max23]
    V4 desc = hn::OddEven(lo, hi);               // [max01,min01,max23,min23]
    V2 asc_lo  = hn::LowerHalf(asc);             // [min01, max01]
    V2 desc_hi = hn::UpperHalf(d2, desc);        // [max23, min23]
    return hn::Combine(d4, desc_hi, asc_lo);
}

// Stride-2 ascending: (0,2)↑(1,3)↑ → [min02, min13, max02, max13]
static inline V4 cas_asc_skip2(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::CombineShiftRightBytes<8>(d4, v, v);  // [c,d,a,b]
    V4 lo = hn::Min(v, rv), hi = hn::Max(v, rv);       // symmetric
    return hn::Combine(d4, hn::UpperHalf(d2, hi), hn::LowerHalf(lo));
}

// Stride-2 descending: (0,2)↓(1,3)↓ → [max02, max13, min02, min13]
static inline V4 cas_desc_skip2(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::CombineShiftRightBytes<8>(d4, v, v);
    V4 lo = hn::Min(v, rv), hi = hn::Max(v, rv);
    return hn::Combine(d4, hn::UpperHalf(d2, lo), hn::LowerHalf(hi));
}

// ================================================================
// Highway bitonic sort — 16 int32 elements, 10 parallel steps
// ================================================================

void bitonic_sort_highway(int* a) {
    const D4 d4;
    V4 v0 = hn::Load(d4, a);
    V4 v1 = hn::Load(d4, a + 4);
    V4 v2 = hn::Load(d4, a + 8);
    V4 v3 = hn::Load(d4, a + 12);

    // ---- Phase 1 (k=2): sort pairs, alternating ↑↓ ----
    // Step 1: (0,1)↑(2,3)↓ in each register
    v0 = cas_mixed_adj(v0);
    v1 = cas_mixed_adj(v1);
    v2 = cas_mixed_adj(v2);
    v3 = cas_mixed_adj(v3);

    // ---- Phase 2 (k=4): build sorted 4-element runs ----
    // Step 2: stride-2  — v0,v2 ascending; v1,v3 descending
    v0 = cas_asc_skip2(v0);    v1 = cas_desc_skip2(v1);
    v2 = cas_asc_skip2(v2);    v3 = cas_desc_skip2(v3);

    // Step 3: adjacent — v0,v2 ascending; v1,v3 descending
    v0 = cas_asc_adj(v0);      v1 = cas_desc_adj(v1);
    v2 = cas_asc_adj(v2);      v3 = cas_desc_adj(v3);

    // ---- Phase 3 (k=8): build sorted 8-element runs ----
    // Step 4: cross-register  (v0,v1)↑  (v2,v3)↓
    { V4 t = hn::Min(v0,v1); v1 = hn::Max(v0,v1); v0 = t; }
    { V4 t = hn::Max(v2,v3); v3 = hn::Min(v2,v3); v2 = t; }

    // Step 5: stride-2 — v0,v1 ascending; v2,v3 descending
    v0 = cas_asc_skip2(v0);    v1 = cas_asc_skip2(v1);
    v2 = cas_desc_skip2(v2);   v3 = cas_desc_skip2(v3);

    // Step 6: adjacent — v0,v1 ascending; v2,v3 descending
    v0 = cas_asc_adj(v0);      v1 = cas_asc_adj(v1);
    v2 = cas_desc_adj(v2);     v3 = cas_desc_adj(v3);

    // ---- Phase 4 (k=16): final merge, all ascending ----
    // Step 7: cross-register  (v0,v2)↑  (v1,v3)↑
    { V4 t = hn::Min(v0,v2); v2 = hn::Max(v0,v2); v0 = t; }
    { V4 t = hn::Min(v1,v3); v3 = hn::Max(v1,v3); v1 = t; }

    // Step 8: cross-register  (v0,v1)↑  (v2,v3)↑
    { V4 t = hn::Min(v0,v1); v1 = hn::Max(v0,v1); v0 = t; }
    { V4 t = hn::Min(v2,v3); v3 = hn::Max(v2,v3); v2 = t; }

    // Step 9: stride-2 all ascending
    v0 = cas_asc_skip2(v0);    v1 = cas_asc_skip2(v1);
    v2 = cas_asc_skip2(v2);    v3 = cas_asc_skip2(v3);

    // Step 10: adjacent all ascending
    v0 = cas_asc_adj(v0);      v1 = cas_asc_adj(v1);
    v2 = cas_asc_adj(v2);      v3 = cas_asc_adj(v3);

    hn::Store(v0, d4, a);
    hn::Store(v1, d4, a + 4);
    hn::Store(v2, d4, a + 8);
    hn::Store(v3, d4, a + 12);
}

void bitonic_sort_highway(vector<int>& v) { bitonic_sort_highway(v.data()); }

// ================================================================
// Scalar bitonic sort (for comparison)
// ================================================================

static inline void cas_asc (int& a, int& b) { int lo=min(a,b),hi=max(a,b); a=lo; b=hi; }
static inline void cas_desc(int& a, int& b) { int lo=min(a,b),hi=max(a,b); a=hi; b=lo; }

void bitonic_sort_scalar(int* a) {
    cas_asc (a[ 0],a[ 1]); cas_desc(a[ 2],a[ 3]);
    cas_asc (a[ 4],a[ 5]); cas_desc(a[ 6],a[ 7]);
    cas_asc (a[ 8],a[ 9]); cas_desc(a[10],a[11]);
    cas_asc (a[12],a[13]); cas_desc(a[14],a[15]);

    cas_asc (a[ 0],a[ 2]); cas_asc (a[ 1],a[ 3]);
    cas_desc(a[ 4],a[ 6]); cas_desc(a[ 5],a[ 7]);
    cas_asc (a[ 8],a[10]); cas_asc (a[ 9],a[11]);
    cas_desc(a[12],a[14]); cas_desc(a[13],a[15]);

    cas_asc (a[ 0],a[ 1]); cas_asc (a[ 2],a[ 3]);
    cas_desc(a[ 4],a[ 5]); cas_desc(a[ 6],a[ 7]);
    cas_asc (a[ 8],a[ 9]); cas_asc (a[10],a[11]);
    cas_desc(a[12],a[13]); cas_desc(a[14],a[15]);

    cas_asc (a[ 0],a[ 4]); cas_asc (a[ 1],a[ 5]);
    cas_asc (a[ 2],a[ 6]); cas_asc (a[ 3],a[ 7]);
    cas_desc(a[ 8],a[12]); cas_desc(a[ 9],a[13]);
    cas_desc(a[10],a[14]); cas_desc(a[11],a[15]);

    cas_asc (a[ 0],a[ 2]); cas_asc (a[ 1],a[ 3]);
    cas_asc (a[ 4],a[ 6]); cas_asc (a[ 5],a[ 7]);
    cas_desc(a[ 8],a[10]); cas_desc(a[ 9],a[11]);
    cas_desc(a[12],a[14]); cas_desc(a[13],a[15]);

    cas_asc (a[ 0],a[ 1]); cas_asc (a[ 2],a[ 3]);
    cas_asc (a[ 4],a[ 5]); cas_asc (a[ 6],a[ 7]);
    cas_desc(a[ 8],a[ 9]); cas_desc(a[10],a[11]);
    cas_desc(a[12],a[13]); cas_desc(a[14],a[15]);

    cas_asc (a[ 0],a[ 8]); cas_asc (a[ 1],a[ 9]);
    cas_asc (a[ 2],a[10]); cas_asc (a[ 3],a[11]);
    cas_asc (a[ 4],a[12]); cas_asc (a[ 5],a[13]);
    cas_asc (a[ 6],a[14]); cas_asc (a[ 7],a[15]);

    cas_asc (a[ 0],a[ 4]); cas_asc (a[ 1],a[ 5]);
    cas_asc (a[ 2],a[ 6]); cas_asc (a[ 3],a[ 7]);
    cas_asc (a[ 8],a[12]); cas_asc (a[ 9],a[13]);
    cas_asc (a[10],a[14]); cas_asc (a[11],a[15]);

    cas_asc (a[ 0],a[ 2]); cas_asc (a[ 1],a[ 3]);
    cas_asc (a[ 4],a[ 6]); cas_asc (a[ 5],a[ 7]);
    cas_asc (a[ 8],a[10]); cas_asc (a[ 9],a[11]);
    cas_asc (a[12],a[14]); cas_asc (a[13],a[15]);

    cas_asc (a[ 0],a[ 1]); cas_asc (a[ 2],a[ 3]);
    cas_asc (a[ 4],a[ 5]); cas_asc (a[ 6],a[ 7]);
    cas_asc (a[ 8],a[ 9]); cas_asc (a[10],a[11]);
    cas_asc (a[12],a[13]); cas_asc (a[14],a[15]);
}

void bitonic_sort_scalar(vector<int>& v) { bitonic_sort_scalar(v.data()); }

// ---------------------------------------------------------------------------
// Measurement harness.
//
// Three machine properties that are easy to ignore will dominate the result
// if you let them:
//
//  1. HETEROGENEOUS CORES.  Apple silicon has performance and efficiency
//     cores; an unpinned run lands wherever the scheduler puts it.
//  2. CLOCK RAMP.  The governor raises the clock in response to instructions
//     per cycle, not to the core being busy, and it takes a moment to do it.
//     A cold first measurement is taken at the base clock.
//  3. THE BRANCH PREDICTOR.  A comparison sort's branches depend on the data.
//     Sort the same array over and over and the predictor learns it, and a
//     branchy sort runs several times faster than it ever will on new data.
//     So every sort gets a different random input.
//
// Set PIN_CPU to move off a busy core.  On Apple silicon there is no numbered
// pinning, but the quality-of-service class selects the cluster: PIN_CPU under
// 4 asks for a performance core.
// ---------------------------------------------------------------------------

static int pin_to_fast_core() {
    int cpu = 0;
    if (const char* e = getenv("PIN_CPU")) cpu = atoi(e);
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        fprintf(stderr, "warning: could not pin to cpu%d\n", cpu);
#elif defined(__APPLE__)
    pthread_set_qos_class_self_np(
        cpu < 4 ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_BACKGROUND, 0);
#endif
    return cpu;
}

// A dependent chain of integer adds retires one per cycle, so this reports the
// clock the core is running at now rather than its nameplate maximum.
static double core_clock_ghz() {
    const long n = 100000000;
    long a = 0;
    auto t0 = steady_clock::now();
    for (long i = 0; i < n / 8; i++)
#if defined(__aarch64__)
        asm volatile("add %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\t"
                     "add %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1\n\tadd %0,%0,#1"
                     : "+r"(a));
#else
        asm volatile("addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\t"
                     "addq $1,%0\n\taddq $1,%0\n\taddq $1,%0\n\taddq $1,%0"
                     : "+r"(a));
#endif
    auto t1 = steady_clock::now();
    return n / duration<double>(t1 - t0).count() / 1e9;
}

// Eight INDEPENDENT chains, so this is high-IPC and the governor responds to
// it.  Spinning on the dependent chain above does not raise the clock.
static double warm_up(double seconds = 0.8) {
    long a0 = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
    auto t0 = steady_clock::now();
    while (duration<double>(steady_clock::now() - t0).count() < seconds)
        for (int i = 0; i < 200000; i++)
#if defined(__aarch64__)
            asm volatile("add %0,%0,#1\n\tadd %1,%1,#1\n\tadd %2,%2,#1\n\tadd %3,%3,#1\n\t"
                         "add %4,%4,#1\n\tadd %5,%5,#1\n\tadd %6,%6,#1\n\tadd %7,%7,#1"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#else
            asm volatile("addq $1,%0\n\taddq $1,%1\n\taddq $1,%2\n\taddq $1,%3\n\t"
                         "addq $1,%4\n\taddq $1,%5\n\taddq $1,%6\n\taddq $1,%7"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),"+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#endif
    return core_clock_ghz();
}

// Best time per sort of n elements, in ns, over RUNS trials.  Each trial
// sorts about 100,000 elements in total, and every sort gets a different
// random input: a pool of up to 4096 distinct arrays, cycled through.  The
// pool is the same for every sort function at a given n, so rows compare like
// with like.  The time includes copying the input into place, under 1 ns at
// n = 16.
template <typename SortFn>
static double bench_sort_ns(int n, SortFn sort_fn) {
    const int reps  = max(1, 100000 / n);
    const int count = min(reps, 4096);
    mt19937 rng(42);
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    vector<int> pool((size_t)count * n);
    for (int& x : pool) x = dist(rng);

    vector<int> work(n);
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        auto t0 = steady_clock::now();
        for (int k = 0; k < reps; k++) {
            const int* src = pool.data() + (size_t)(k % count) * n;
            copy(src, src + n, work.begin());
            sort_fn(work);
            asm volatile("" ::"r"(work.data()) : "memory");
        }
        auto t1 = steady_clock::now();
        best = min(best, duration<double>(t1 - t0).count() * 1e9 / reps);
    }
    return best;
}

static void print_setup(int cpu, double ghz) {
    printf("cpu%d, %.2f GHz after warm-up (measured), best of %d trials,"
           " a different random input for every sort\n\n", cpu, ghz, RUNS);
}

// Three significant figures: 8.52 ns, 63.8 ns, 137 ns, 1.61 ms.
static string fmt_time(double ns) {
    const char* unit = "ns";
    if (ns >= 1e9)      { ns /= 1e9; unit = "s";  }
    else if (ns >= 1e6) { ns /= 1e6; unit = "ms"; }
    else if (ns >= 1e3) { ns /= 1e3; unit = "µs"; }
    char buf[32];
    snprintf(buf, sizeof buf, "%.*f %s", ns < 10 ? 2 : ns < 100 ? 1 : 0, ns, unit);
    return buf;
}

int main() {
    int cpu = pin_to_fast_core();
    double ghz = warm_up();
    print_setup(cpu, ghz);

    mt19937 rng(42);
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);

    // Correctness checks
    {
        vector<int> v = {15,3,9,1,7,13,5,11,2,14,6,10,4,8,12,0};
        vector<int> ref = v;
        sort(ref.begin(), ref.end());
        bitonic_sort_highway(v);
        cout << "Correctness (fixed):  " << (v == ref ? "PASS" : "FAIL") << "\n";

        vector<int> v2(N);
        for (int& x : v2) x = dist(rng);
        vector<int> ref2 = v2;
        sort(ref2.begin(), ref2.end());
        bitonic_sort_highway(v2);
        cout << "Correctness (random): " << (v2 == ref2 ? "PASS" : "FAIL") << "\n";
        cout << "HWY target: " << hwy::TargetName(HWY_TARGET) << "\n\n";
    }

    double ns_hwy    = bench_sort_ns(N, [](vector<int>& v){ bitonic_sort_highway(v); });
    double ns_scalar = bench_sort_ns(N, [](vector<int>& v){ bitonic_sort_scalar(v); });

    double nlog2n = (double)N * log2((double)N);

    cout << string(72, '-') << "\n";
    cout << "Bitonic sort  n=16  10 steps  80 CAS   Highway vs scalar\n";
    cout << string(72, '-') << "\n";
    cout << left  << setw(32) << "version"
         << right << setw(10) << "time"
         << right << setw(14) << "ns/elem"
         << right << setw(16) << "ns/(n log2 n)" << "\n";
    cout << string(72, '-') << "\n";

    auto row = [&](const char* label, double ns) {
        cout << left  << setw(32) << label
             << right << setw(10) << fmt_time(ns)
             << right << setw(13) << fixed << setprecision(1) << ns / N
             << right << setw(15) << fixed << setprecision(2) << ns / nlog2n
             << "\n";
    };

    row("scalar (branchless min/max)", ns_scalar);
    row("Highway SIMD",                ns_hwy);

    cout << string(72, '-') << "\n";
    cout << fixed << setprecision(2)
         << "SIMD speedup: " << ns_scalar / max(1.0, ns_hwy) << "x\n";
    cout << "\nEach Highway step processes 4 comparators in parallel.\n";
    cout << "Portable: same source compiles to NEON, SSE4, AVX2, AVX-512.\n";

    return 0;
}
