// Quicksort: where the time goes, by partition size
//
// Instruments quicksort.cpp's quicksort (median-of-three, no small-partition
// cutoff) to answer one question: how much of its time is spent in small
// partitions -- the part an insertion-sort cutoff would replace?
//
// S(T) is the time spent inside OUTERMOST calls on partitions of <= T
// elements.  A call on a partition of <= T elements is timed as a whole --
// its entire subtree, run uninstrumented -- so nothing inside it is timed
// again or counted twice.  Each timed call costs a pair of clock reads (10-13 ns
// on an Apple M5, by compiler), and a small subtree takes only a few times that, so the
// cost is measured once and subtracted from every timed call.
//
// Two tables:
//   1. S(16) as a share of the whole sort, for n = 32 to 1M.  (At n = 16 the
//      root is itself a <= 16 partition, so the share is 100% by definition.)
//   2. For one sort of 1M elements, the time in each band of partition sizes,
//      from successive differences S(T_i) - S(T_{i-1}), with the number of
//      calls in each band (calls on 2 or more elements).
//
// Every sort gets a different random input, as in the other benchmarks here.
// timing_size_breakdown.md and three_regimes.html report its output.
//
// Build:
//   clang++ -O2 -std=c++17 quicksort_breakdown.cpp -o quicksort_breakdown && ./quicksort_breakdown
// With GCC and libstdc++ (Homebrew's g++-16; /usr/bin/g++ on a Mac is clang):
//   g++-16 -O2 -std=c++17 -isysroot $(xcrun --show-sdk-path) quicksort_breakdown.cpp -o quicksort_breakdown

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
    // a[lo] <= a[mid] <= a[hi]; place pivot at hi-1
    swap(a[mid], a[hi - 1]);
    return a[hi - 1];
}

static void quicksort(int* a, int lo, int hi) {
    if (hi <= lo) return;
    if (hi - lo == 1) {            // two elements: one compare suffices
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
    swap(a[i], a[hi - 1]);   // restore pivot
    quicksort(a, lo, i - 1);
    quicksort(a, i + 1, hi);
}

void quicksort(vector<int>& v) {
    if (v.size() > 1)
        quicksort(v.data(), 0, (int)v.size() - 1);
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

// ================================================================
// Instrumentation
// ================================================================

static long   g_T;          // time outermost calls on partitions of <= g_T
static double g_ns;         // total time inside those calls
static long   g_entries;    // how many were timed
static double g_clock_ns;   // cost of one pair of steady_clock::now() calls

// quicksort() above, except that a partition of <= g_T elements is handed to
// the uninstrumented quicksort() and timed as a whole.
static void quicksort_timed(int* a, int lo, int hi) {
    if (hi - lo + 1 <= g_T) {
        auto t0 = steady_clock::now();
        quicksort(a, lo, hi);
        auto t1 = steady_clock::now();
        g_ns += duration<double>(t1 - t0).count() * 1e9;
        g_entries++;
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
    quicksort_timed(a, lo, i - 1);
    quicksort_timed(a, i + 1, hi);
}

// Upper bounds of the partition-size bands for table 2.
static const long BANDS[] = {16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};
static const int  NBANDS  = sizeof BANDS / sizeof BANDS[0];
static long g_calls[NBANDS];

// quicksort() above, counting calls on 2 or more elements by band.
static void quicksort_counted(int* a, int lo, int hi) {
    long size = hi - lo + 1;
    if (size >= 2) {
        int b = 0;
        while (size > BANDS[b]) b++;
        g_calls[b]++;
    }
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
    quicksort_counted(a, lo, i - 1);
    quicksort_counted(a, i + 1, hi);
}

static double clock_pair_ns() {
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        const int k = 1000000;
        double sum = 0;
        for (int i = 0; i < k; i++) {
            auto t0 = steady_clock::now();
            auto t1 = steady_clock::now();
            sum += duration<double>(t1 - t0).count() * 1e9;
        }
        best = min(best, sum / k);
    }
    return best;
}

// For sorts of n elements: the best (over RUNS trials) time per sort, plain,
// and the best S(T) per sort with the clock cost subtracted.  Same input pool
// as bench_sort_ns in the other benchmarks.
static void measure(int n, long T, double& total_ns, double& small_ns) {
    const int reps  = max(1, 100000 / n);
    const int count = min(reps, 4096);
    mt19937 rng(42);
    uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
    vector<int> pool((size_t)count * n);
    for (int& x : pool) x = dist(rng);

    vector<int> work(n);
    total_ns = small_ns = 1e300;
    for (int r = 0; r < RUNS; r++) {
        auto t0 = steady_clock::now();
        for (int k = 0; k < reps; k++) {
            const int* src = pool.data() + (size_t)(k % count) * n;
            copy(src, src + n, work.begin());
            quicksort(work);
            asm volatile("" ::"r"(work.data()) : "memory");
        }
        auto t1 = steady_clock::now();
        total_ns = min(total_ns, duration<double>(t1 - t0).count() * 1e9 / reps);
    }
    for (int r = 0; r < RUNS; r++) {
        g_T = T;
        g_ns = 0;
        g_entries = 0;
        for (int k = 0; k < reps; k++) {
            const int* src = pool.data() + (size_t)(k % count) * n;
            copy(src, src + n, work.begin());
            quicksort_timed(work.data(), 0, n - 1);
            asm volatile("" ::"r"(work.data()) : "memory");
        }
        small_ns = min(small_ns, (g_ns - g_entries * g_clock_ns) / reps);
    }
}

int main() {
    int cpu = pin_to_fast_core();
    double ghz = warm_up();
    g_clock_ns = clock_pair_ns();
    printf("cpu%d, %.2f GHz after warm-up (measured), best of %d trials,"
           " a different random input for every sort\n", cpu, ghz, RUNS);
    printf("a pair of clock reads costs %.1f ns; subtracted from every timed call\n\n",
           g_clock_ns);

    cout << string(66, '-') << "\n";
    cout << "quicksort: share of time in partitions of <= 16 elements\n";
    cout << string(66, '-') << "\n";
    cout << right << setw(10) << "n"
         << setw(12) << "time"
         << setw(12) << "ns/elem"
         << setw(16) << "ns/(n log2 n)"
         << setw(14) << "<= 16 share" << "\n";
    cout << string(66, '-') << "\n";
    for (int e = 5; e <= 20; e++) {
        int n = 1 << e;
        double total, small;
        measure(n, 16, total, small);
        cout << right << setw(10) << n
             << setw(12) << fmt_time(total)
             << setw(12) << fixed << setprecision(1) << total / n
             << setw(16) << fixed << setprecision(2) << total / (n * log2((double)n))
             << setw(13) << fixed << setprecision(1) << 100 * small / total << "%"
             << "\n";
    }
    cout << string(66, '-') << "\n\n";

    // Table 2: one sort of 1M elements, by band.  The input is the first
    // array of measure()'s pool, so the call counts match the timed sorts.
    const int n = 1 << 20;
    {
        mt19937 rng(42);
        uniform_int_distribution<int> dist(INT_MIN, INT_MAX);
        vector<int> v(n);
        for (int& x : v) x = dist(rng);
        quicksort_counted(v.data(), 0, n - 1);
    }
    double S[NBANDS];
    for (int b = 0; b < NBANDS; b++) {
        double total;
        measure(n, BANDS[b], total, S[b]);
    }
    const double whole = S[NBANDS - 1];   // the root call, timed as a whole

    cout << string(58, '-') << "\n";
    cout << "quicksort, n = " << n << ": time by partition size ("
         << fmt_time(whole) << " in all)\n";
    cout << string(58, '-') << "\n";
    cout << left << setw(22) << "partition size" << right
         << setw(10) << "calls"
         << setw(14) << "excl. time"
         << setw(12) << "share" << "\n";
    cout << string(58, '-') << "\n";
    double prev = 0;
    for (int b = 0; b < NBANDS; b++) {
        double band = S[b] - prev;
        prev = S[b];
        string label = b == 0 ? "<= 16"
                              : to_string(BANDS[b - 1] + 1) + " - " + to_string(BANDS[b]);
        cout << left << setw(22) << label << right
             << setw(10) << g_calls[b]
             << setw(14) << fmt_time(band)
             << setw(11) << fixed << setprecision(1) << 100 * band / whole << "%\n";
    }
    cout << string(58, '-') << "\n";
    return 0;
}
