// Quicksort vs std::sort — large-array comparison
//
// Runs both algorithms on the same random inputs across sizes 2^10 to 2^20
// (1K to 1M elements).
//
// std::sort is an introsort: quicksort + insertion-sort for small partitions
// + heapsort fallback for worst-case depth.  libc++ adds one more thing for
// arithmetic types compared with the default operator<: a BRANCH-FREE bitset
// partition, which records the comparisons against the pivot in bitmasks and
// swaps from those, so no branch depends on the data.
//
// Two facts about libc++ on macOS shape how this is measured:
//
//  1. std::sort on int with the default comparison does not compile the sort
//     into this program.  libc++ declares std::__sort<__less<int>&, int*> as
//     an extern template, so the call goes to a copy precompiled into the
//     system's libc++.1.dylib.
//  2. A sort compiled from this SDK's headers never takes the bitset
//     partition, even with a comparator libc++ lists as branch-free such as
//     std::greater: its dispatch checks the comparator without the '&' that
//     its list of branch-free comparators expects.  Only the precompiled
//     copies get it.
//
// So the controls call libc++'s own __introsort directly, compiled from the
// header, with the bitset-partition flag forced on and off.  Everything else
// -- cutoff, pivot choice, the 3/4/5-element networks, the heapsort fallback
// -- is identical between the two, so their ratio is the partition alone.
// These are libc++ internals and the call is specific to this libc++ version.
//
// Build:
//   clang++ -O2 -std=c++17 sort_compare_large.cpp -o sort_compare_large && ./sort_compare_large

#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <climits>
#include <string>
#include <cmath>
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

static const int RUNS = 7;

// ================================================================
// Quicksort — median-of-three pivot, no small-partition cutoff
// ================================================================

static int median3(int* a, int lo, int hi) {
    int mid = lo + (hi - lo) / 2;
    if (a[lo] > a[mid]) swap(a[lo], a[mid]);
    if (a[lo] > a[hi])  swap(a[lo], a[hi]);
    if (a[mid] > a[hi]) swap(a[mid], a[hi]);
    swap(a[mid], a[hi - 1]);
    return a[hi - 1];
}

static void qs(int* a, int lo, int hi) {
    if (hi <= lo) return;
    if (hi - lo == 1) {
        if (a[lo] > a[hi]) swap(a[lo], a[hi]);
        return;
    }
    int pivot = median3(a, lo, hi);
    int i = lo, j = hi - 1;
    for (;;) {
        while (a[++i] < pivot) {}
        while (a[--j] > pivot) {}
        if (i >= j) break;
        swap(a[i], a[j]);
    }
    swap(a[i], a[hi - 1]);
    qs(a, lo, i - 1);
    qs(a, i + 1, hi);
}

static void quicksort(vector<int>& v) {
    if (v.size() > 1) qs(v.data(), 0, (int)v.size() - 1);
}

static void std_sort_wrap(vector<int>& v) {
    sort(v.begin(), v.end());
}

// libc++'s introsort compiled from the header, bitset partition on or off.
template <bool BITSET>
static void libcxx_introsort(vector<int>& v) {
    std::__less<> comp;
    int* first = v.data();
    int* last  = first + v.size();
    std::__introsort<std::_ClassicAlgPolicy, std::__less<>&, int*, BITSET>(
        first, last, comp, 2 * std::__log2i(last - first));
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

    // 2^10 to 2^20
    vector<int> sizes;
    for (int e = 10; e <= 20; e++) sizes.push_back(1 << e);

    // Correctness check
    {
        vector<int> v = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
        quicksort(v);
        cout << "Quicksort correctness: " << (is_sorted(v.begin(), v.end()) ? "PASS" : "FAIL") << "\n\n";
    }

    const int W = 13;
    cout << string(86, '-') << "\n";
    cout << "quicksort (median-of-3) vs std::sort (introsort)   random int data\n";
    cout << string(86, '-') << "\n";
    cout << right
         << setw(10) << "n"
         << setw(W)  << "quicksort"
         << setw(W)  << "std::sort"
         << setw(W)  << "bitset on"
         << setw(W)  << "bitset off"
         << setw(W)  << "qs / std"
         << setw(W)  << "off / on"
         << "\n";
    cout << string(86, '-') << "\n";

    for (int n : sizes) {
        double qs_ns  = bench_sort_ns(n, quicksort);
        double std_ns = bench_sort_ns(n, std_sort_wrap);
        double on_ns  = bench_sort_ns(n, libcxx_introsort<true>);
        double off_ns = bench_sort_ns(n, libcxx_introsort<false>);

        cout << right
             << setw(10) << n
             << setw(W)  << fmt_time(qs_ns)
             << setw(W)  << fmt_time(std_ns)
             << setw(W)  << fmt_time(on_ns)
             << setw(W)  << fmt_time(off_ns)
             << setw(W - 1) << fixed << setprecision(2) << qs_ns / std_ns << "x"
             << setw(W - 1) << fixed << setprecision(2) << off_ns / on_ns << "x"
             << "\n";
    }

    cout << string(86, '-') << "\n";
    cout << "std::sort:  the precompiled copy in libc++.1.dylib\n";
    cout << "bitset on/off: libc++'s __introsort from the header, partition flag forced\n";
    cout << "off / on:   what the branch-free partition alone is worth\n";
    return 0;
}
