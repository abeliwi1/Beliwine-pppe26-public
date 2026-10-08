// Vectorized Quicksort — Algorithms 3 and 4 from Bramas (arXiv:1704.08579),
// written with Google Highway for portable SIMD.
//
// Paper: "A Novel Hybrid Quicksort Algorithm Vectorized using AVX-512 on Intel Skylake"
// The paper partitions with AVX-512's compress-store (_mm512_mask_compressstoreu_epi32).
// This version uses Highway's CompressStore, which maps to:
//   vpcompressd           on AVX-512  (16 int32 lanes)
//   table permute + store on AVX2     ( 8 int32 lanes)
//   table permute + store on NEON     ( 4 int32 lanes, 128-bit)
//
// Everything EXCEPT the partition follows libc++'s std::sort, so comparing the
// two measures the partition and nothing else:
//   - partitions of fewer than 24 elements are handed to std::sort, which at
//     that size runs libc++'s own base case (insertion sort, plus branch-free
//     networks for exactly 3, 4 or 5 elements);
//   - the pivot is libc++'s: median of three, or a median of medians of three
//     above 128 elements, from the same positions;
//   - recursion deeper than 2*log2(n) switches to heapsort, as libc++ does;
//   - the left side is recursed into and the right side looped on.
// libc++ partitions with its branch-free bitset partition; vqs partitions with
// Highway's CompressStore into two scratch buffers.
//
// Not reproduced: libc++'s check for an already-partitioned range and its
// separate partition for runs of keys equal to the pivot.  Both matter on
// presorted or duplicate-heavy input, not on the random data measured here.
//
// Scratch buffers (2 x n ints) are allocated once at the public entry point and
// threaded through the recursion to avoid per-call heap allocation.
//
// Build:
//   clang++ -O2 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib \
//     vqs_highway.cpp -o vqs_highway -lhwy && ./vqs_highway

#include "hwy/highway.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>
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

static constexpr int RUNS              = 7;
static constexpr int INSERTION_LIMIT   = 24;   // libc++: std::sort below this size
static constexpr int NINTHER_THRESHOLD = 128;  // libc++: median of medians above this

// ================================================================
// simd_partition — Algorithm 3 of the paper: the one part of this sort that
// differs from std::sort.
//
// Partitions a[lo..hi) — hi is EXCLUSIVE; the pivot is NOT in this range.
// Loads N elements per iteration, compares them against the broadcast pivot,
// and CompressStore-scatters elements < pivot into left_buf and elements
// >= pivot into right_buf.  A scalar loop handles the tail.  Writes both
// buffers back contiguously to a[lo..hi) and returns lc, the number of
// elements < pivot.
// ================================================================
static int simd_partition(int* a, int lo, int hi, int pivot,
                           int* left_buf, int* right_buf) {
    const HWY_FULL(int32_t) d;
    const int N = (int)hn::Lanes(d);

    const auto pivot_v = hn::Set(d, pivot);
    int lc = 0, rc = 0;

    // Vectorized loop — process N elements per iteration
    const int vend = lo + ((hi - lo) / N) * N;
    for (int i = lo; i < vend; i += N) {
        const auto v       = hn::LoadU(d, a + i);
        const auto lt_mask = hn::Lt(v, pivot_v);   // v[i] < pivot
        const auto ge_mask = hn::Ge(v, pivot_v);   // v[i] >= pivot
        lc += (int)hn::CompressStore(v, lt_mask, d, left_buf  + lc);
        rc += (int)hn::CompressStore(v, ge_mask, d, right_buf + rc);
    }
    // Scalar tail for the remaining < N elements
    for (int i = vend; i < hi; i++) {
        if (a[i] < pivot) left_buf[lc++] = a[i];
        else              right_buf[rc++] = a[i];
    }

    // Write classified elements back in order: [left | right]
    memcpy(a + lo,      left_buf,  lc * sizeof(int));
    memcpy(a + lo + lc, right_buf, rc * sizeof(int));
    return lc;
}

// ================================================================
// Pivot selection, as libc++ does it.  sort3 orders three elements in place;
// libc++ uses the same (branchy) three-element sort for its pivot.  The pivot
// ends up at a[lo].
// ================================================================
static inline void sort3(int* a, int i, int j, int k) {   // a[i] <= a[j] <= a[k]
    if (a[j] < a[i]) swap(a[i], a[j]);
    if (a[k] < a[j]) {
        swap(a[j], a[k]);
        if (a[j] < a[i]) swap(a[i], a[j]);
    }
}

static inline void choose_pivot(int* a, int lo, int hi) {
    const int len = hi - lo + 1, half = len / 2;
    if (len > NINTHER_THRESHOLD) {             // median of medians of three
        sort3(a, lo,     lo + half,     hi);
        sort3(a, lo + 1, lo + half - 1, hi - 1);
        sort3(a, lo + 2, lo + half + 1, hi - 2);
        sort3(a, lo + half - 1, lo + half, lo + half + 1);
        swap(a[lo], a[lo + half]);
    } else {                                   // median of three
        sort3(a, lo + half, lo, hi);
    }
}

// ================================================================
// simd_qs_core — Algorithm 4: the recursive driver, shaped like libc++'s
// __introsort.  Sorts a[lo..hi] (inclusive).
// ================================================================
static void simd_qs_core(int* a, int lo, int hi, int depth,
                         int* left_buf, int* right_buf) {
    while (true) {
        const int len = hi - lo + 1;
        if (len < INSERTION_LIMIT) {           // libc++'s base case, exactly
            std::sort(a + lo, a + hi + 1);
            return;
        }
        if (depth == 0) {                      // too deep: heapsort, as libc++
            std::partial_sort(a + lo, a + hi + 1, a + hi + 1);
            return;
        }
        --depth;

        choose_pivot(a, lo, hi);
        const int pivot = a[lo];

        // Partition a[lo+1..hi] around the pivot: [ < pivot | >= pivot ]
        const int lc = simd_partition(a, lo + 1, hi + 1, pivot, left_buf, right_buf);

        // Move the pivot between the two groups: the last element < pivot
        // trades places with it.
        const int p = lo + lc;
        swap(a[lo], a[p]);

        simd_qs_core(a, lo, p - 1, depth, left_buf, right_buf);   // recurse left
        lo = p + 1;                                                // loop right
    }
}

// ================================================================
// Public entry point
// ================================================================
void vqs_sort(vector<int>& v) {
    const int n = (int)v.size();
    if (n < INSERTION_LIMIT) { std::sort(v.begin(), v.end()); return; }
    vector<int> left_buf(n), right_buf(n);
    simd_qs_core(v.data(), 0, n - 1, 2 * (int)log2((double)n),
                 left_buf.data(), right_buf.data());
}

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

    // --- Correctness ---
    {
        auto check = [](vector<int> v, const char* label) {
            vector<int> ref = v;
            sort(ref.begin(), ref.end());
            vqs_sort(v);
            cout << label << (v == ref ? "PASS" : "FAIL") << "\n";
        };
        mt19937 r2(99);
        uniform_int_distribution<int> d2(INT_MIN, INT_MAX);
        vector<int> big(100000), sorted_big(100000), equal_big(100000, 7);
        for (int& x : big) x = d2(r2);
        for (int i = 0; i < 100000; i++) sorted_big[i] = i;
        check({5, 3, 8, 1, 9, 2, 7, 4, 6, 0}, "Correctness (n=10):            ");
        check({42},                           "Correctness (n=1):             ");
        check(big,                            "Correctness (n=100k random):   ");
        check(sorted_big,                     "Correctness (n=100k sorted):   ");
        check(equal_big,                      "Correctness (n=100k all-equal):");

        cout << "HWY target: " << hwy::TargetName(HWY_TARGET) << "\n\n";
    }

    // --- Benchmark: vqs_highway vs std::sort ---
    auto std_fn = [](vector<int>& v) { sort(v.begin(), v.end()); };
    auto vqs_fn = [](vector<int>& v) { vqs_sort(v); };

    constexpr int W = 62;
    cout << string(W, '-') << "\n";
    cout << "vqs_highway vs std::sort   (only the partition differs)\n";
    cout << string(W, '-') << "\n";
    cout << right << setw(10) << "n"
         << right << setw(12) << "vqs"
         << right << setw(12) << "std::sort"
         << right << setw(10) << "speedup"
         << right << setw(16) << "ns/(n log2 n)" << "\n";
    cout << string(W, '-') << "\n";

    for (int e = 5; e <= 20; e++) {
        const int n = 1 << e;
        const double ns_v  = bench_sort_ns(n, vqs_fn);
        const double ns_s  = bench_sort_ns(n, std_fn);
        const double nlogn = (double)n * log2((double)n);

        cout << right << setw(10) << n
             << right << setw(12) << fmt_time(ns_v)
             << right << setw(12) << fmt_time(ns_s)
             << right << setw(9)  << fixed << setprecision(2) << ns_s / ns_v << "x"
             << right << setw(15) << fixed << setprecision(2) << ns_v / nlogn
             << "\n";
    }

    cout << string(W, '-') << "\n";
    {
        const HWY_FULL(int32_t) d;
        cout << "std::sort below " << INSERTION_LIMIT
             << "  VEC_LANES=" << hn::Lanes(d)
             << "  target=" << hwy::TargetName(HWY_TARGET) << "\n";
    }
    return 0;
}
