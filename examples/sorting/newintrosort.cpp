// Quicksort (median-of-3) + SIMD bitonic base case, no heapsort fallback
//
// - Recurse with quicksort (median-of-three pivot)
// - Switch to bitonic_sort_simd for partitions of 16 or fewer elements
//
// No depth limit or heapsort fallback — worst-case O(n²) is possible on
// adversarial input, though median-of-three makes it very unlikely in practice.
// See newintrosort_heapsortfallback.cpp for the O(n log n) guaranteed variant.
//
// Build:
//   clang++ -O2 -std=c++17 newintrosort.cpp -o newintrosort && ./newintrosort

#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <climits>
#include <string>
#include <arm_neon.h>
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

static const int RUNS   = 7;
static const int CUTOFF = 16;

// ================================================================
// SIMD bitonic sort — exactly 16 int32 elements, 10 parallel steps
// (ARM NEON: 4 × int32x4_t replace one AVX-512 ZMM register)
// ================================================================

alignas(16) static const uint32_t MASK_MIXED [4] = {~0u, 0,   0,   ~0u};
alignas(16) static const uint32_t MASK_ASC_A [4] = {~0u, 0,   ~0u, 0  };
alignas(16) static const uint32_t MASK_DESC_A[4] = {0,   ~0u, 0,   ~0u};
alignas(16) static const uint32_t MASK_ASC_S [4] = {~0u, ~0u, 0,   0  };
alignas(16) static const uint32_t MASK_DESC_S[4] = {0,   0,   ~0u, ~0u};

static inline int32x4_t cas_adj(int32x4_t v, uint32x4_t sel) {
    int32x4_t rv = vrev64q_s32(v);
    return vbslq_s32(sel, vminq_s32(v, rv), vmaxq_s32(v, rv));
}

static inline int32x4_t cas_skip2(int32x4_t v, uint32x4_t sel) {
    int32x4_t rv = vextq_s32(v, v, 2);
    return vbslq_s32(sel, vminq_s32(v, rv), vmaxq_s32(v, rv));
}

static void bitonic_sort_simd(int* a) {
    int32x4_t v0 = vld1q_s32(a),      v1 = vld1q_s32(a + 4);
    int32x4_t v2 = vld1q_s32(a + 8),  v3 = vld1q_s32(a + 12);

    uint32x4_t m_mixed  = vld1q_u32(MASK_MIXED);
    uint32x4_t m_asc_a  = vld1q_u32(MASK_ASC_A);
    uint32x4_t m_desc_a = vld1q_u32(MASK_DESC_A);
    uint32x4_t m_asc_s  = vld1q_u32(MASK_ASC_S);
    uint32x4_t m_desc_s = vld1q_u32(MASK_DESC_S);

    // Phase 1 (k=2)
    v0 = cas_adj(v0, m_mixed);  v1 = cas_adj(v1, m_mixed);
    v2 = cas_adj(v2, m_mixed);  v3 = cas_adj(v3, m_mixed);

    // Phase 2 (k=4)
    v0 = cas_skip2(v0, m_asc_s);  v1 = cas_skip2(v1, m_desc_s);
    v2 = cas_skip2(v2, m_asc_s);  v3 = cas_skip2(v3, m_desc_s);
    v0 = cas_adj(v0, m_asc_a);    v1 = cas_adj(v1, m_desc_a);
    v2 = cas_adj(v2, m_asc_a);    v3 = cas_adj(v3, m_desc_a);

    // Phase 3 (k=8)
    int32x4_t t;
    t = vminq_s32(v0, v1); v1 = vmaxq_s32(v0, v1); v0 = t;
    t = vmaxq_s32(v2, v3); v3 = vminq_s32(v2, v3); v2 = t;
    v0 = cas_skip2(v0, m_asc_s);  v1 = cas_skip2(v1, m_asc_s);
    v2 = cas_skip2(v2, m_desc_s); v3 = cas_skip2(v3, m_desc_s);
    v0 = cas_adj(v0, m_asc_a);    v1 = cas_adj(v1, m_asc_a);
    v2 = cas_adj(v2, m_desc_a);   v3 = cas_adj(v3, m_desc_a);

    // Phase 4 (k=16)
    t = vminq_s32(v0, v2); v2 = vmaxq_s32(v0, v2); v0 = t;
    t = vminq_s32(v1, v3); v3 = vmaxq_s32(v1, v3); v1 = t;
    t = vminq_s32(v0, v1); v1 = vmaxq_s32(v0, v1); v0 = t;
    t = vminq_s32(v2, v3); v3 = vmaxq_s32(v2, v3); v2 = t;
    v0 = cas_skip2(v0, m_asc_s);  v1 = cas_skip2(v1, m_asc_s);
    v2 = cas_skip2(v2, m_asc_s);  v3 = cas_skip2(v3, m_asc_s);
    v0 = cas_adj(v0, m_asc_a);    v1 = cas_adj(v1, m_asc_a);
    v2 = cas_adj(v2, m_asc_a);    v3 = cas_adj(v3, m_asc_a);

    vst1q_s32(a,      v0);  vst1q_s32(a + 4,  v1);
    vst1q_s32(a + 8,  v2);  vst1q_s32(a + 12, v3);
}

// Partition of n ≤ 16: pad scratch to 16 with INT_MAX, sort, copy n back.
// INT_MAX padding is safe because it sinks to the tail of the sorted result.
static void sort_small(int* a, int n) {
    if (n <= 1) return;
    alignas(16) int buf[16];
    for (int i = 0;  i < n;  i++) buf[i] = a[i];
    for (int i = n; i < 16; i++) buf[i] = INT_MAX;
    bitonic_sort_simd(buf);
    for (int i = 0; i < n; i++) a[i] = buf[i];
}

// ================================================================
// Quicksort with median-of-three pivot
// ================================================================

static int median3(int* a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) swap(a[lo], a[mid]);
    if (a[lo] > a[hi])  swap(a[lo], a[hi]);
    if (a[mid] > a[hi]) swap(a[mid], a[hi]);
    swap(a[mid], a[hi - 1]);
    return a[hi - 1];
}

static void introsort_rec(int* a, int lo, int hi) {
    int n = hi - lo + 1;
    if (n <= CUTOFF) { sort_small(a + lo, n); return; }

    int pivot = median3(a, lo, hi);
    int i = lo, j = hi - 1;
    for (;;) {
        while (a[++i] < pivot) {}
        while (a[--j] > pivot) {}
        if (i >= j) break;
        swap(a[i], a[j]);
    }
    swap(a[i], a[hi - 1]);
    introsort_rec(a, lo,    i - 1);
    introsort_rec(a, i + 1, hi);
}

void introsort(vector<int>& v) {
    int n = (int)v.size();
    if (n > 1)
        introsort_rec(v.data(), 0, n - 1);
}

// ================================================================
// Reference algorithms (same data, fair comparison)
// ================================================================

static void median3_qs_rec(int* a, int lo, int hi) {
    if (hi <= lo) return;
    if (hi - lo == 1) { if (a[lo] > a[hi]) swap(a[lo], a[hi]); return; }
    int pivot = median3(a, lo, hi);
    int i = lo, j = hi - 1;
    for (;;) {
        while (a[++i] < pivot) {}
        while (a[--j] > pivot) {}
        if (i >= j) break;
        swap(a[i], a[j]);
    }
    swap(a[i], a[hi - 1]);
    median3_qs_rec(a, lo, i - 1);
    median3_qs_rec(a, i + 1, hi);
}

static void quicksort(vector<int>& v) {
    if (v.size() > 1) median3_qs_rec(v.data(), 0, (int)v.size() - 1);
}

static void std_sort(vector<int>& v) { sort(v.begin(), v.end()); }

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

    // Correctness
    {
        auto check = [](const char* label, void (*fn)(vector<int>&)) {
            vector<int> v = {9,3,7,1,5,15,11,13,2,14,6,10,4,8,12,0};
            vector<int> ref = v;
            sort(ref.begin(), ref.end());
            fn(v);
            cout << label << ": " << (v == ref ? "PASS" : "FAIL") << "\n";
        };
        check("introsort n=16", introsort);

        // larger random check
        vector<int> big(100000);
        for (int& x : big) x = dist(rng);
        vector<int> ref = big;
        sort(ref.begin(), ref.end());
        introsort(big);
        cout << "introsort n=100000: " << (big == ref ? "PASS" : "FAIL") << "\n\n";
    }

    // 2^10 to 2^20
    vector<int> sizes;
    for (int e = 10; e <= 20; e++) sizes.push_back(1 << e);

    const int W = 14;
    cout << string(84, '-') << "\n";
    cout << "introsort (qs + SIMD bitonic ≤16) vs quicksort vs std::sort   random int32\n";
    cout << string(84, '-') << "\n";
    cout << right
         << setw(10) << "n"
         << setw(W)  << "introsort"
         << setw(W)  << "quicksort"
         << setw(W)  << "std::sort"
         << setw(12) << "vs qs"
         << setw(10) << "vs std"
         << "\n";
    cout << string(84, '-') << "\n";

    for (int n : sizes) {
        double is_ns  = bench_sort_ns(n, introsort);
        double qs_ns  = bench_sort_ns(n, quicksort);
        double std_ns = bench_sort_ns(n, std_sort);

        // ratio < 1.0 means introsort is faster than the reference
        double r_qs  = (double)is_ns / (double)qs_ns;
        double r_std = (double)is_ns / (double)std_ns;

        cout << right
             << setw(10) << n
             << setw(W)  << fmt_time(is_ns)
             << setw(W)  << fmt_time(qs_ns)
             << setw(W)  << fmt_time(std_ns)
             << setw(11) << fixed << setprecision(2) << r_qs  << "x"
             << setw(9)  << fixed << setprecision(2) << r_std << "x"
             << "\n";
    }

    cout << string(84, '-') << "\n";
    cout << "ratio < 1.0 → introsort faster; ratio > 1.0 → introsort slower\n";
    return 0;
}
