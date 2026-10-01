// activity2_conflicts.cpp  —  STARTER
//
// Predicting cache set conflicts in a matrix transpose.
//
// Build and run:
//   g++ -std=c++17 -O1 -o activity2 activity2_conflicts.cpp && ./activity2
//
// Pin to a specific core with PIN_CPU (see Part 4):
//   PIN_CPU=0 ./activity2
//   PIN_CPU=4 ./activity2

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <cstring>
#include <string>
#include <vector>
#if defined(__linux__)
  #include <sched.h>
#elif defined(__APPLE__)
  #include <sys/sysctl.h>
  #include <pthread.h>
  #include <sys/qos.h>
#endif

using namespace std;
using namespace std::chrono;

static const int N    = 2048;   // logical matrix is always N x N
static const int RUNS = 5;

// ===================================================================
// Machine geometry, read from the kernel rather than assumed.
// ===================================================================

struct Geometry {
    int size_bytes = 0;   // L1d capacity
    int ways       = 0;   // associativity
    int line       = 0;   // line size in bytes
    int sets       = 0;   // size / (ways * line)
};

static int read_int(const string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return 0;
    int v = 0;
    // sizes come back like "48K"; the K is the unit, not a digit
    char buf[64] = {0};
    if (fgets(buf, sizeof buf, f)) {
        v = atoi(buf);
        if (strchr(buf, 'K')) v *= 1024;
        else if (strchr(buf, 'M')) v *= 1024 * 1024;
    }
    fclose(f);
    return v;
}

static string read_str(const string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return "";
    char buf[64] = {0};
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return ""; }
    fclose(f);
    string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == ' ')) s.pop_back();
    return s;
}

// Associativity is the one number macOS will not tell you, so measure it:
// chase a pointer around W lines that all map to the same set.  While W fits
// in the ways, every hop is an L1 hit; one line past it, every hop misses.
// The stride is a generous power of two so that it is a whole number of
// set-cycles on any plausible geometry.
static int measure_ways(int line_bytes) {
    const size_t stride = 64 * 1024;
    const int    maxw   = 32;
    vector<char> buf(stride * (size_t)(maxw + 2));
    char* base = buf.data();
    double first = 0.0;

    for (int w = 1; w <= maxw; w++) {
        for (int k = 0; k < w; k++)
            *(void**)(base + (size_t)k * stride) = base + (size_t)((k + 1) % w) * stride;

        void** q = (void**)base;
        for (long i = 0; i < 200000; i++) q = (void**)*q;      // warm
        auto t0 = steady_clock::now();
        for (long i = 0; i < 2000000; i++) q = (void**)*q;
        auto t1 = steady_clock::now();
        asm volatile("" :: "r"(q));

        double ns = duration<double, nano>(t1 - t0).count() / 2000000.0;
        if (w == 1) first = ns;
        if (ns > first * 2.0) return w - 1;                    // fell out of L1
    }
    (void)line_bytes;
    return maxw;
}

#if defined(__linux__)
// Walk cpu0's cache index directories and pick out the level-1 data cache.
static Geometry read_geometry(int cpu) {
    Geometry g;
    for (int idx = 0; idx < 10; idx++) {
        string base = "/sys/devices/system/cpu/cpu" + to_string(cpu)
                    + "/cache/index" + to_string(idx) + "/";
        string type = read_str(base + "type");
        if (type.empty()) continue;
        if (read_int(base + "level") != 1) continue;
        if (type != "Data" && type != "Unified") continue;
        g.size_bytes = read_int(base + "size");
        g.ways       = read_int(base + "ways_of_associativity");
        g.line       = read_int(base + "coherency_line_size");
        break;
    }
    return g;
}
#elif defined(__APPLE__)
static long sysctl_long(const char* name) {
    long v = 0; size_t sz = sizeof v;
    if (sysctlbyname(name, &v, &sz, nullptr, 0) != 0) return 0;
    return v;
}
// macOS exposes size and line size but never associativity, so that one is
// measured.  perflevel0 is the performance core cluster; plain hw.l1dcachesize
// reports the efficiency cores and is a factor of two off for our purposes.
static Geometry read_geometry(int) {
    Geometry g;
    g.size_bytes = (int)sysctl_long("hw.perflevel0.l1dcachesize");
    if (g.size_bytes == 0) g.size_bytes = (int)sysctl_long("hw.l1dcachesize");
    g.line       = (int)sysctl_long("hw.cachelinesize");
    g.ways       = 0;                       // filled in by measure_ways below
    return g;
}
#else
static Geometry read_geometry(int) { return Geometry{}; }
#endif

// Anything the OS would not say can be supplied by hand, which also lets you
// check your formula against a machine you do not have in front of you:
//   L1_SIZE=131072 L1_WAYS=8 L1_LINE=128 ./activity2
static Geometry get_geometry(int cpu) {
    Geometry g = read_geometry(cpu);
    if (const char* e = getenv("L1_SIZE")) g.size_bytes = atoi(e);
    if (const char* e = getenv("L1_LINE")) g.line       = atoi(e);
    if (const char* e = getenv("L1_WAYS")) g.ways       = atoi(e);
    if (g.ways == 0 && g.line > 0) {
        fprintf(stderr, "associativity not reported by the OS; measuring it...\n");
        g.ways = measure_ways(g.line);
    }
    if (g.ways > 0 && g.line > 0) g.sets = g.size_bytes / (g.ways * g.line);
    return g;
}

// ===================================================================
// PART 2 — the prediction.  See the handout; you implement this.

int sets_reachable(int lda, const Geometry& g) {
    // TODO (Part 2): return the number of distinct L1 sets that one column
    // of an N x lda matrix of doubles touches.
    //
    // Everything you need is in `g`: g.line, g.sets, g.ways.
    // std::gcd is available from <numeric>.
    //
    // Work it out in this order:
    //   1. How many BYTES does stepping down one row advance the address?
    //   2. How many cache LINES is that?
    //   3. By how many SETS does the set index advance per step?
    //   4. Repeatedly adding that advance modulo g.sets, how many distinct
    //      set numbers do you visit before repeating?
    //
    // Two cases are easy to get wrong; think about both before you code:
    //   - What if the row is not a whole number of cache lines?
    //   - What if the set index advances by exactly 0 per step?
    (void)lda; (void)g;
    return 1;   // <-- replace
}

// ===================================================================
// Benchmark harness
// ===================================================================

static void clobber() { asm volatile("" ::: "memory"); }

static int pin_to_core() {
    int cpu = 0;
    if (const char* e = getenv("PIN_CPU")) cpu = atoi(e);
#if defined(__linux__)
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0)
        fprintf(stderr, "warning: could not pin to cpu%d\n", cpu);
#elif defined(__APPLE__)
    // Apple silicon gives no way to pin to a numbered core, but the quality
    // of service class does choose the cluster, which is what Part 4 needs.
    // PIN_CPU < 4 asks for a performance core, anything else an efficiency one.
    pthread_set_qos_class_self_np(
        cpu < 4 ? QOS_CLASS_USER_INTERACTIVE : QOS_CLASS_BACKGROUND, 0);
#endif
    return cpu;
}

// A dependent chain of ADDs retires one per cycle, so this reports the clock
// the core is running at right now rather than its nameplate maximum.
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

// The governor raises the clock in response to instructions-per-cycle, not to
// the core merely being busy.  A transpose is memory-bound and low-IPC, so
// left alone it runs the whole way at the base clock -- and every repetition
// is equally slow, so min-of-RUNS does not rescue you.  Spin on genuinely
// parallel work first.
static double warm_up(double seconds = 0.8) {
    long a0=0,a1=0,a2=0,a3=0,a4=0,a5=0,a6=0,a7=0;
    auto t0 = steady_clock::now();
    while (duration<double>(steady_clock::now() - t0).count() < seconds)
        for (int i = 0; i < 200000; i++)
#if defined(__aarch64__)
            asm volatile("add %0,%0,#1\n\tadd %1,%1,#1\n\tadd %2,%2,#1\n\tadd %3,%3,#1\n\t"
                         "add %4,%4,#1\n\tadd %5,%5,#1\n\tadd %6,%6,#1\n\tadd %7,%7,#1"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),
                           "+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#else
            asm volatile("addq $1,%0\n\taddq $1,%1\n\taddq $1,%2\n\taddq $1,%3\n\t"
                         "addq $1,%4\n\taddq $1,%5\n\taddq $1,%6\n\taddq $1,%7"
                         : "+r"(a0),"+r"(a1),"+r"(a2),"+r"(a3),
                           "+r"(a4),"+r"(a5),"+r"(a6),"+r"(a7));
#endif
    return core_clock_ghz();
}

// Naive transpose: reads a row (stride 1), writes a column (stride lda).
static void transpose(const vector<double>& in, vector<double>& out, int lda) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            out[(size_t)j * lda + i] = in[(size_t)i * lda + j];
}

static double bench(int lda, double& sink) {
    vector<double> in((size_t)N * lda), out((size_t)N * lda);
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            in[(size_t)i * lda + j] = i * N + j;

    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        clobber();
        auto t0 = steady_clock::now();
        clobber();
        transpose(in, out, lda);
        clobber();
        auto t1 = steady_clock::now();
        clobber();
        best = min(best, duration<double, milli>(t1 - t0).count());
    }
    sink += out[0];
    return best;
}

int main() {
    int cpu = pin_to_core();
    double ghz = warm_up();

    Geometry g = get_geometry(cpu);
    if (g.sets == 0) {
        fprintf(stderr, "could not determine L1d geometry.\n"
                "Supply it by hand, e.g. L1_SIZE=131072 L1_WAYS=8 L1_LINE=128\n");
        return 1;
    }

    printf("cpu%d  %.2f GHz (measured, after warm-up)\n", cpu, ghz);
    printf("L1d: %d KB, %d-way, %d-byte lines  ->  %d sets\n\n",
           g.size_bytes / 1024, g.ways, g.line, g.sets);

    printf("matrix %dx%d doubles, naive transpose\n\n", N, N);
    printf("%8s %11s %9s %10s %11s\n",
           "lda", "row bytes", "sets", "time", "vs lda=N");
    printf("%8s %11s %9s %10s %11s\n",
           "--------", "-----------", "---------", "----------", "-----------");

    double sink = 0.0, base = 0.0;
    int ldas[] = { 2048, 2049, 2050, 2052, 2056, 2064, 2080, 2112, 2176, 2304 };

    for (int lda : ldas) {
        double ms = bench(lda, sink);
        if (lda == N) base = ms;
        printf("%8d %11ld %9d %8.1f ms %10.2fx\n",
               lda, (long)lda * 8, sets_reachable(lda, g), ms, base / ms);
        fflush(stdout);
    }

    printf("\n(checksum %.0f)\n", sink);
    printf("core clock at end of run: %.2f GHz\n", core_clock_ghz());
    return 0;
}
