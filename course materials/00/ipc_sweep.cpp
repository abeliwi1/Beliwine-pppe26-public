// IPC sweep: how many independent integer adds can this core sustain per cycle?
//
// Integer `add` has 1-cycle latency (unlike the 3-cycle FADD used elsewhere
// in ILP/), so a single accumulator chain is *already* dependency-bound at
// 1 add/cycle -- it needs no help hiding latency. The only way to push IPC
// higher is genuine port parallelism: K independent chains, each landing on
// a (possibly different) integer ALU port each cycle. Sweeping K reveals
// where that stops paying off -- the actual sustained ALU throughput of this
// core, as distinct from its 10-wide front-end issue width.
//
// The source array is small (fits L1) and re-read many times ("passes") so
// the loop is ALU-bound, not memory-bound -- unlike out_of_order.cpp, which
// deliberately runs large enough to hit a memory-bandwidth floor.
//
// Build: clang++ -O1 -o ipc_sweep ipc_sweep.cpp && ./ipc_sweep
// (-O1, not -O2: avoids auto-vectorization folding the adds into NEON ops)

#include <chrono>
#include <cstdio>
#include <climits>
#include <algorithm>
#include <vector>

using namespace std::chrono;

// Divisible by every K tested below (LCM(1,2,4,6,8,12,16) = 48) so no
// variant reads past the end of the array.
static const long long ARR_N  = 8160;          // 8160 * 8B = ~64 KB: fits L1
static const long long PASSES = 500000;         // total adds/pass = ARR_N
static const int RUNS = 5;

// Apple M5 performance core (see ../../branch_optimizations/branch_free.cpp).
static const double CPU_GHZ = 4.6;

template <typename Func>
long long bench_ns(Func f) {
    long long best = LLONG_MAX;
    for (int r = 0; r < RUNS; r++) {
        auto t0 = high_resolution_clock::now();
        f();
        auto t1 = high_resolution_clock::now();
        long long ns = duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        best = std::min(best, ns);
    }
    return best;
}

template <typename T>
static void sink(T const& v) { asm volatile("" : : "m"(v) : "memory"); }

// Each sum_k re-reads the same ARR_N-element array `passes` times, splitting
// it into K independent accumulator chains. Total adds executed is ARR_N *
// passes regardless of K -- only how that work is distributed changes.

long long sum_1(const long long* a, long long passes) {
    long long s0=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 1) { s0 += a[i]; }
    return s0;
}
long long sum_2(const long long* a, long long passes) {
    long long s0=0,s1=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 2) { s0 += a[i]; s1 += a[i+1]; }
    return s0+s1;
}
long long sum_4(const long long* a, long long passes) {
    long long s0=0,s1=0,s2=0,s3=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 4) {
            s0 += a[i]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
        }
    return s0+s1+s2+s3;
}
long long sum_6(const long long* a, long long passes) {
    long long s0=0,s1=0,s2=0,s3=0,s4=0,s5=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 6) {
            s0 += a[i]; s1 += a[i+1]; s2 += a[i+2];
            s3 += a[i+3]; s4 += a[i+4]; s5 += a[i+5];
        }
    return s0+s1+s2+s3+s4+s5;
}
long long sum_8(const long long* a, long long passes) {
    long long s0=0,s1=0,s2=0,s3=0,s4=0,s5=0,s6=0,s7=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 8) {
            s0 += a[i]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
            s4 += a[i+4]; s5 += a[i+5]; s6 += a[i+6]; s7 += a[i+7];
        }
    return s0+s1+s2+s3+s4+s5+s6+s7;
}
long long sum_12(const long long* a, long long passes) {
    long long s0=0,s1=0,s2=0,s3=0,s4=0,s5=0,s6=0,s7=0,s8=0,s9=0,s10=0,s11=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 12) {
            s0 += a[i]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
            s4 += a[i+4]; s5 += a[i+5]; s6 += a[i+6]; s7 += a[i+7];
            s8 += a[i+8]; s9 += a[i+9]; s10 += a[i+10]; s11 += a[i+11];
        }
    return s0+s1+s2+s3+s4+s5+s6+s7+s8+s9+s10+s11;
}
long long sum_16(const long long* a, long long passes) {
    long long s0=0,s1=0,s2=0,s3=0,s4=0,s5=0,s6=0,s7=0;
    long long s8=0,s9=0,s10=0,s11=0,s12=0,s13=0,s14=0,s15=0;
    for (long long p = 0; p < passes; p++)
        for (long long i = 0; i < ARR_N; i += 16) {
            s0 += a[i]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
            s4 += a[i+4]; s5 += a[i+5]; s6 += a[i+6]; s7 += a[i+7];
            s8 += a[i+8]; s9 += a[i+9]; s10 += a[i+10]; s11 += a[i+11];
            s12 += a[i+12]; s13 += a[i+13]; s14 += a[i+14]; s15 += a[i+15];
        }
    return s0+s1+s2+s3+s4+s5+s6+s7+s8+s9+s10+s11+s12+s13+s14+s15;
}

int main() {
    std::vector<long long> a(ARR_N);
    for (long long i = 0; i < ARR_N; i++) a[i] = (i % 97) + 1; // nonzero, non-trivial

    struct Variant { const char* name; int k; long long (*fn)(const long long*, long long); };
    Variant variants[] = {
        {"1 accumulator",  1,  sum_1},
        {"2 accumulators", 2,  sum_2},
        {"4 accumulators", 4,  sum_4},
        {"6 accumulators", 6,  sum_6},
        {"8 accumulators", 8,  sum_8},
        {"12 accumulators",12, sum_12},
        {"16 accumulators",16, sum_16},
    };

    double total_adds = (double)ARR_N * (double)PASSES; // constant across K

    printf("Integer add sweep: %.0f adds total per variant (%lld elems x %lld passes), CPU=%.1f GHz (assumed)\n",
           total_adds, ARR_N, PASSES, CPU_GHZ);
    printf("%-18s %10s %12s %8s %8s\n", "version", "time(ms)", "cyc/add", "IPC", "vs K=1");
    printf("--------------------------------------------------------------\n");

    double base_cpe = 0.0;
    for (auto& v : variants) {
        long long ns = bench_ns([&]{ sink(v.fn(a.data(), PASSES)); });
        double cyc_per_add = (double)ns * CPU_GHZ / total_adds; // GHz == cycles/ns
        double ipc = 1.0 / cyc_per_add;
        if (v.k == 1) base_cpe = cyc_per_add;
        printf("%-18s %10.1f %12.4f %8.2f %7.2fx\n",
               v.name, ns / 1e6, cyc_per_add, ipc, base_cpe / cyc_per_add);
    }

    printf("\nIPC here counts only the accumulator `add`s themselves, not the\n"
           "loop's own index-increment/compare/branch overhead -- so read it as\n"
           "a lower bound on achievable ALU throughput, not an absolute ceiling.\n"
           "Where IPC stops climbing as K grows is this core's sustained integer\n"
           "ALU width -- compare that to the 10-wide front-end figure quoted in\n"
           "../../ILP/speculative_execution.md.\n");

    return 0;
}
