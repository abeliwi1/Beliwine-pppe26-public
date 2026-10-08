// Sorting Algorithm Comparison at n=16
//
// Runs six sort implementations on the same 16-element random int array
// and prints a single speedup table normalised to quicksort.
//
// Implementations:
//   std::sort      — libc++ introsort
//   quicksort      — recursive median-of-3, no small-partition cutoff
//   insertion sort — classic O(n²) in-place sort
//   bitonic scalar — branchless min/max network, 80 CAS ops
//   bitonic NEON   — ARM NEON port of arXiv:1704.08579 Algorithm 1
//   bitonic Highway— Google Highway portable SIMD (same algorithm)
//
// Build:
//   clang++ -O2 -std=c++17 -I/opt/homebrew/include -L/opt/homebrew/lib \
//     sort_compare_n16.cpp -o sort_compare_n16 -lhwy && ./sort_compare_n16

#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <climits>
#include <string>
#include <sstream>
#include <cmath>
#include <arm_neon.h>
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
// std::sort
// ================================================================

void sort_std(vector<int>& v) { sort(v.begin(), v.end()); }

// ================================================================
// Quicksort — median-of-3, no small-partition cutoff
// ================================================================

static int qs_median3(int* a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) swap(a[lo], a[mid]);
    if (a[lo] > a[hi])  swap(a[lo], a[hi]);
    if (a[mid] > a[hi]) swap(a[mid], a[hi]);
    swap(a[mid], a[hi - 1]);
    return a[hi - 1];
}

static void qs_impl(int* a, int lo, int hi) {
    if (hi <= lo) return;
    if (hi - lo == 1) { if (a[lo] > a[hi]) swap(a[lo], a[hi]); return; }
    int pivot = qs_median3(a, lo, hi);
    int i = lo, j = hi - 1;
    for (;;) {
        while (a[++i] < pivot) {}
        while (a[--j] > pivot) {}
        if (i >= j) break;
        swap(a[i], a[j]);
    }
    swap(a[i], a[hi - 1]);
    qs_impl(a, lo, i - 1);
    qs_impl(a, i + 1, hi);
}

void sort_quicksort(vector<int>& v) {
    if (v.size() > 1) qs_impl(v.data(), 0, (int)v.size() - 1);
}

// ================================================================
// Insertion sort
// ================================================================

void sort_insertion(vector<int>& v) {
    int n = v.size();
    for (int i = 1; i < n; i++) {
        int key = v[i], j = i - 1;
        while (j >= 0 && v[j] > key) { v[j + 1] = v[j]; j--; }
        v[j + 1] = key;
    }
}

// ================================================================
// Bitonic sort — scalar branchless min/max, 10 steps, 80 CAS
// ================================================================

static inline void cas_asc (int& a, int& b) { int lo=min(a,b),hi=max(a,b); a=lo; b=hi; }
static inline void cas_desc(int& a, int& b) { int lo=min(a,b),hi=max(a,b); a=hi; b=lo; }

void sort_bitonic_scalar(int* a) {
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

void sort_bitonic_scalar(vector<int>& v) { sort_bitonic_scalar(v.data()); }

// ================================================================
// Bitonic sort — ARM NEON (arXiv:1704.08579 Algorithm 1 port)
// ================================================================

alignas(16) static const uint32_t MASK_MIXED [4] = {~0u, 0,   0,   ~0u};
alignas(16) static const uint32_t MASK_ASC_A [4] = {~0u, 0,   ~0u, 0  };
alignas(16) static const uint32_t MASK_DESC_A[4] = {0,   ~0u, 0,   ~0u};
alignas(16) static const uint32_t MASK_ASC_S [4] = {~0u, ~0u, 0,   0  };
alignas(16) static const uint32_t MASK_DESC_S[4] = {0,   0,   ~0u, ~0u};

static inline int32x4_t neon_cas_adj(int32x4_t v, uint32x4_t sel) {
    int32x4_t rv = vrev64q_s32(v);
    return vbslq_s32(sel, vminq_s32(v, rv), vmaxq_s32(v, rv));
}

static inline int32x4_t neon_cas_skip2(int32x4_t v, uint32x4_t sel) {
    int32x4_t rv = vextq_s32(v, v, 2);
    return vbslq_s32(sel, vminq_s32(v, rv), vmaxq_s32(v, rv));
}

void sort_bitonic_neon(int* a) {
    int32x4_t v0 = vld1q_s32(a),      v1 = vld1q_s32(a + 4);
    int32x4_t v2 = vld1q_s32(a + 8),  v3 = vld1q_s32(a + 12);
    uint32x4_t mm = vld1q_u32(MASK_MIXED),  ma = vld1q_u32(MASK_ASC_A);
    uint32x4_t md = vld1q_u32(MASK_DESC_A), ms = vld1q_u32(MASK_ASC_S);
    uint32x4_t mds= vld1q_u32(MASK_DESC_S);

    v0=neon_cas_adj(v0,mm);  v1=neon_cas_adj(v1,mm);   // step 1
    v2=neon_cas_adj(v2,mm);  v3=neon_cas_adj(v3,mm);

    v0=neon_cas_skip2(v0,ms);  v1=neon_cas_skip2(v1,mds); // step 2
    v2=neon_cas_skip2(v2,ms);  v3=neon_cas_skip2(v3,mds);

    v0=neon_cas_adj(v0,ma);  v1=neon_cas_adj(v1,md);    // step 3
    v2=neon_cas_adj(v2,ma);  v3=neon_cas_adj(v3,md);

    int32x4_t t;                                         // step 4
    t=vminq_s32(v0,v1); v1=vmaxq_s32(v0,v1); v0=t;
    t=vmaxq_s32(v2,v3); v3=vminq_s32(v2,v3); v2=t;

    v0=neon_cas_skip2(v0,ms);  v1=neon_cas_skip2(v1,ms);  // step 5
    v2=neon_cas_skip2(v2,mds); v3=neon_cas_skip2(v3,mds);

    v0=neon_cas_adj(v0,ma);  v1=neon_cas_adj(v1,ma);    // step 6
    v2=neon_cas_adj(v2,md);  v3=neon_cas_adj(v3,md);

    t=vminq_s32(v0,v2); v2=vmaxq_s32(v0,v2); v0=t;     // step 7
    t=vminq_s32(v1,v3); v3=vmaxq_s32(v1,v3); v1=t;

    t=vminq_s32(v0,v1); v1=vmaxq_s32(v0,v1); v0=t;     // step 8
    t=vminq_s32(v2,v3); v3=vmaxq_s32(v2,v3); v2=t;

    v0=neon_cas_skip2(v0,ms);  v1=neon_cas_skip2(v1,ms); // step 9
    v2=neon_cas_skip2(v2,ms);  v3=neon_cas_skip2(v3,ms);

    v0=neon_cas_adj(v0,ma);  v1=neon_cas_adj(v1,ma);   // step 10
    v2=neon_cas_adj(v2,ma);  v3=neon_cas_adj(v3,ma);

    vst1q_s32(a,v0); vst1q_s32(a+4,v1); vst1q_s32(a+8,v2); vst1q_s32(a+12,v3);
}

void sort_bitonic_neon(vector<int>& v) { sort_bitonic_neon(v.data()); }

// ================================================================
// Bitonic sort — Google Highway (portable SIMD)
// ================================================================

using D4 = hn::FixedTag<int32_t, 4>;
using D2 = hn::Half<D4>;
using V4 = hn::Vec<D4>;
using V2 = hn::Vec<D2>;

static inline V4 hwy_cas_asc_adj(V4 v) {
    const D4 d4;
    V4 rv = hn::Reverse2(d4, v), lo = hn::Min(v,rv), hi = hn::Max(v,rv);
    return hn::OddEven(hi, lo);
}
static inline V4 hwy_cas_desc_adj(V4 v) {
    const D4 d4;
    V4 rv = hn::Reverse2(d4, v), lo = hn::Min(v,rv), hi = hn::Max(v,rv);
    return hn::OddEven(lo, hi);
}
static inline V4 hwy_cas_mixed_adj(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::Reverse2(d4, v), lo = hn::Min(v,rv), hi = hn::Max(v,rv);
    V4 asc = hn::OddEven(hi,lo), desc = hn::OddEven(lo,hi);
    return hn::Combine(d4, hn::UpperHalf(d2,desc), hn::LowerHalf(asc));
}
static inline V4 hwy_cas_asc_skip2(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::CombineShiftRightBytes<8>(d4,v,v), lo = hn::Min(v,rv), hi = hn::Max(v,rv);
    return hn::Combine(d4, hn::UpperHalf(d2,hi), hn::LowerHalf(lo));
}
static inline V4 hwy_cas_desc_skip2(V4 v) {
    const D4 d4; const D2 d2;
    V4 rv = hn::CombineShiftRightBytes<8>(d4,v,v), lo = hn::Min(v,rv), hi = hn::Max(v,rv);
    return hn::Combine(d4, hn::UpperHalf(d2,lo), hn::LowerHalf(hi));
}

void sort_bitonic_highway(int* a) {
    const D4 d4;
    V4 v0=hn::Load(d4,a), v1=hn::Load(d4,a+4), v2=hn::Load(d4,a+8), v3=hn::Load(d4,a+12);

    v0=hwy_cas_mixed_adj(v0);  v1=hwy_cas_mixed_adj(v1);   // step 1
    v2=hwy_cas_mixed_adj(v2);  v3=hwy_cas_mixed_adj(v3);

    v0=hwy_cas_asc_skip2(v0);  v1=hwy_cas_desc_skip2(v1);  // step 2
    v2=hwy_cas_asc_skip2(v2);  v3=hwy_cas_desc_skip2(v3);

    v0=hwy_cas_asc_adj(v0);    v1=hwy_cas_desc_adj(v1);    // step 3
    v2=hwy_cas_asc_adj(v2);    v3=hwy_cas_desc_adj(v3);

    { V4 t=hn::Min(v0,v1); v1=hn::Max(v0,v1); v0=t; }      // step 4
    { V4 t=hn::Max(v2,v3); v3=hn::Min(v2,v3); v2=t; }

    v0=hwy_cas_asc_skip2(v0);  v1=hwy_cas_asc_skip2(v1);   // step 5
    v2=hwy_cas_desc_skip2(v2); v3=hwy_cas_desc_skip2(v3);

    v0=hwy_cas_asc_adj(v0);    v1=hwy_cas_asc_adj(v1);     // step 6
    v2=hwy_cas_desc_adj(v2);   v3=hwy_cas_desc_adj(v3);

    { V4 t=hn::Min(v0,v2); v2=hn::Max(v0,v2); v0=t; }      // step 7
    { V4 t=hn::Min(v1,v3); v3=hn::Max(v1,v3); v1=t; }

    { V4 t=hn::Min(v0,v1); v1=hn::Max(v0,v1); v0=t; }      // step 8
    { V4 t=hn::Min(v2,v3); v3=hn::Max(v2,v3); v2=t; }

    v0=hwy_cas_asc_skip2(v0);  v1=hwy_cas_asc_skip2(v1);   // step 9
    v2=hwy_cas_asc_skip2(v2);  v3=hwy_cas_asc_skip2(v3);

    v0=hwy_cas_asc_adj(v0);    v1=hwy_cas_asc_adj(v1);     // step 10
    v2=hwy_cas_asc_adj(v2);    v3=hwy_cas_asc_adj(v3);

    hn::Store(v0,d4,a); hn::Store(v1,d4,a+4);
    hn::Store(v2,d4,a+8); hn::Store(v3,d4,a+12);
}

void sort_bitonic_highway(vector<int>& v) { sort_bitonic_highway(v.data()); }

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

    // Correctness check — all six implementations on same input
    {
        vector<int> v = {15,3,9,1,7,13,5,11,2,14,6,10,4,8,12,0};
        vector<int> ref = v;
        sort(ref.begin(), ref.end());
        bool ok = true;
        vector<int> t;
        t=v; sort_std(t);            ok &= (t==ref);
        t=v; sort_quicksort(t);      ok &= (t==ref);
        t=v; sort_insertion(t);      ok &= (t==ref);
        t=v; sort_bitonic_scalar(t); ok &= (t==ref);
        t=v; sort_bitonic_neon(t);   ok &= (t==ref);
        t=v; sort_bitonic_highway(t);ok &= (t==ref);
        cout << "Correctness: " << (ok ? "PASS" : "FAIL")
             << "  (HWY target: " << hwy::TargetName(HWY_TARGET) << ")\n\n";
    }

    struct Entry { const char* name; double ns; };
    vector<Entry> results = {
        { "std::sort (introsort)",      bench_sort_ns(N, sort_std)             },
        { "quicksort (median-of-3)",    bench_sort_ns(N, sort_quicksort)       },
        { "insertion sort",             bench_sort_ns(N, sort_insertion)        },
        { "bitonic scalar (80 CAS)",    bench_sort_ns(N, [](vector<int>& v){ sort_bitonic_scalar(v); }) },
        { "bitonic NEON",               bench_sort_ns(N, [](vector<int>& v){ sort_bitonic_neon(v);   }) },
        { "bitonic Highway",            bench_sort_ns(N, [](vector<int>& v){ sort_bitonic_highway(v);}) },
    };

    double qs_ns = results[1].ns;   // quicksort is the baseline

    cout << string(72, '-') << "\n";
    cout << "Sorting algorithms at n=16  (random int32, best of 7 trials)\n";
    cout << string(72, '-') << "\n";
    cout << left  << setw(30) << "algorithm"
         << right << setw(8)  << "time"
         << right << setw(12) << "ns/elem"
         << right << setw(14) << "vs quicksort" << "\n";
    cout << string(72, '-') << "\n";

    for (auto& e : results) {
        double speedup = qs_ns / e.ns;
        ostringstream su;
        su << fixed << setprecision(2);
        if (speedup >= 1.0) su << speedup << "x faster";
        else                su << (1.0/speedup) << "x slower";
        cout << left  << setw(30) << e.name
             << right << setw(8)  << fmt_time(e.ns)
             << right << setw(11) << fixed << setprecision(1) << e.ns / N
             << right << setw(16) << su.str() << "\n";
    }

    cout << string(72, '-') << "\n";
    cout << "Bitonic networks have no branches and no data-dependent control flow.\n";
    cout << "SIMD versions execute 4 comparators per instruction.\n";
    return 0;
}
