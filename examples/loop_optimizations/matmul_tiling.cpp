// Loop Tiling, part 2: Matrix Multiply — the case tiling is actually for
//
// loop_tiling.cpp uses a transpose, which has NO DATA REUSE: every element is
// read once and written once.  Tiling there can only improve spatial locality
// and defeat set conflicts, and padding the leading dimension does that better.
//
// Matrix multiply is the other shape entirely.  It does O(N^3) work on O(N^2)
// data, so every element of B is read N times.  That is what tiling exists to
// exploit: blocking the iteration space turns those N re-reads into one read
// plus N-1 cache hits, and cuts memory traffic by roughly the tile size.
// Padding cannot do that at any tile size -- the two are independent.
//
//   transpose        N^2 work / N^2 data   reuse factor 1    -> pad it
//   matrix multiply  N^3 work / N^2 data   reuse factor N    -> tile it
//
// The baseline here is already the i,k,j order, so the loop-interchange win
// (see loop_interchange.md) is spent before we start.  Everything measured
// below is what BLOCKING buys on top of the best non-blocked loop order.
//
// BUILD WITH -O3.  This matters more than usual and the example is dishonest
// without it:
//
//   g++ -O3 -march=native -std=c++17 -o matmul_tiling matmul_tiling.cpp
//
// The inner loop is `c[j] += a * b[j]`.  At -O2 GCC uses the "very-cheap"
// vectoriser cost model and refuses it, so every version runs scalar at about
// 1.3 flops/cycle -- which makes the whole kernel memory-bound and leaves
// tiling nothing to win.  Measured on THIS source at n=2048: every blocked
// version lands within 4% of the unblocked one at -O2, and blocking wins 2.98x
// at -O3.  One flag decides whether the example demonstrates anything at all.
// `__restrict` is required for the same reason: without it the compiler cannot
// rule out c and b aliasing, and declines to vectorise.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <sched.h>
#include <cmath>
#include <iomanip>

using namespace std;
using namespace std::chrono;

static const int RUNS = 3;

#ifndef BUILD_LABEL
#  ifdef __OPTIMIZE__
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


// ================================================================
// The kernels.  One body, reused at every blocking level, so the only
// difference between them is which loops are blocked.
// ================================================================

#define MM_BODY(ilo, ihi, klo, khi, jlo, jhi)                    \
    for (int i = (ilo); i < (ihi); i++)                          \
        for (int k = (klo); k < (khi); k++) {                    \
            double a = A[(size_t)i * n + k];                     \
            const double* __restrict b = &B[(size_t)k * n];      \
            double* __restrict c = &C[(size_t)i * n];            \
            for (int j = (jlo); j < (jhi); j++) c[j] += a * b[j]; \
        }

// The textbook-order kernel, for reference: the inner loop walks DOWN a column
// of B, one full row between elements.  This is what loop interchange fixes.
static void mm_ijk(const double* __restrict A, const double* __restrict B,
                   double* __restrict C, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[(size_t)i*n+k] * B[(size_t)k*n+j];
            C[(size_t)i*n+j] += s;
        }
}

// The strong baseline: i,k,j, so the inner loop is stride-1 in both B and C.
static void mm_ikj(const double* __restrict A, const double* __restrict B,
                   double* __restrict C, int n) {
    MM_BODY(0, n, 0, n, 0, n)
}

// One extra loop.  The inner loop now sweeps a strip of columns instead of the
// whole row, so the piece of B it touches is small enough to stay in cache
// while every row of A reuses it.
static void mm_block_j(const double* __restrict A, const double* __restrict B,
                       double* __restrict C, int n, int T) {
    for (int jj = 0; jj < n; jj += T) {
        int jl = min(jj + T, n);
        MM_BODY(0, n, 0, n, jj, jl)
    }
}

static void mm_block_ij(const double* __restrict A, const double* __restrict B,
                        double* __restrict C, int n, int T) {
    for (int ii = 0; ii < n; ii += T) {
        int il = min(ii + T, n);
        for (int jj = 0; jj < n; jj += T) {
            int jl = min(jj + T, n);
            MM_BODY(ii, il, 0, n, jj, jl)
        }
    }
}

// All three blocked.  Note the loop order: kk is INSIDE jj, so the C tile is
// loaded once and accumulated into across the whole kk sweep.  Putting kk
// outside re-reads the C tile n/T times and throws the win away.
static void mm_block_ijk(const double* __restrict A, const double* __restrict B,
                         double* __restrict C, int n, int T) {
    for (int ii = 0; ii < n; ii += T) {
        int il = min(ii + T, n);
        for (int jj = 0; jj < n; jj += T) {
            int jl = min(jj + T, n);
            for (int kk = 0; kk < n; kk += T) {
                int kl = min(kk + T, n);
                MM_BODY(ii, il, kk, kl, jj, jl)
            }
        }
    }
}

// ================================================================
// main
// ================================================================

static double run_one(int which, const double* A, const double* B, double* C,
                      int n, int T) {
    size_t sz = (size_t)n * n;
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        memset(C, 0, sz * sizeof(double));
        clobber();
        auto t0 = steady_clock::now();
        clobber();
        switch (which) {
            case 0: mm_ijk      (A, B, C, n);    break;
            case 1: mm_ikj      (A, B, C, n);    break;
            case 2: mm_block_j  (A, B, C, n, T); break;
            case 3: mm_block_ij (A, B, C, n, T); break;
            default:mm_block_ijk(A, B, C, n, T); break;
        }
        clobber();
        auto t1 = steady_clock::now();
        clobber();
        best = min(best, duration<double, milli>(t1 - t0).count());
    }
    return best;
}

int main(int argc, char** argv) {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();

    int T = (argc > 1) ? atoi(argv[1]) : 256;

    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";
    cout << "Tile size T = " << T << "\n";

    // Correctness: every version must agree with the textbook one.
    {
        int n = 256; size_t sz = (size_t)n * n;
        vector<double> A(sz), B(sz), ref(sz), C(sz);
        for (size_t i = 0; i < sz; i++) { A[i] = (double)(i % 7) * 0.5; B[i] = (double)(i % 11) * 0.25; }
        mm_ijk(A.data(), B.data(), ref.data(), n);
        bool ok = true;
        for (int w = 1; w <= 4 && ok; w++) {
            fill(C.begin(), C.end(), 0.0);
            switch (w) {
                case 1: mm_ikj      (A.data(), B.data(), C.data(), n);     break;
                case 2: mm_block_j  (A.data(), B.data(), C.data(), n, 64); break;
                case 3: mm_block_ij (A.data(), B.data(), C.data(), n, 64); break;
                case 4: mm_block_ijk(A.data(), B.data(), C.data(), n, 64); break;
            }
            for (size_t i = 0; i < sz; i++)
                if (fabs(C[i] - ref[i]) > 1e-6 * fabs(ref[i]) + 1e-9) { ok = false; break; }
        }
        cout << "Correctness (all versions vs i,j,k at n=256): "
             << (ok ? "PASS" : "FAIL") << "\n";
        if (!ok) return 1;
    }

    const char* names[] = { "i,j,k  (textbook order)", "i,k,j  (interchanged)",
                            "  + block j", "  + block i,j", "  + block i,j,k" };

    for (int n : {512, 1024, 2048}) {
        size_t sz = (size_t)n * n;
        double gflop = 2.0 * n * n * (double)n / 1e9;

        // 64-byte aligned, so every row starts on a cache-line boundary and
        // the vector loads never straddle one.
        double* A = (double*)aligned_alloc(64, sz * sizeof(double));
        double* B = (double*)aligned_alloc(64, sz * sizeof(double));
        double* C = (double*)aligned_alloc(64, sz * sizeof(double));
        if (!A || !B || !C) { cerr << "allocation failed at n=" << n << "\n"; return 1; }
        for (size_t i = 0; i < sz; i++) { A[i] = (double)(i % 7) * 0.5; B[i] = (double)(i % 11) * 0.25; }

        cout << "\n" << string(66, '-') << "\n";
        cout << "n = " << n << "   " << (sz * 8 / (1 << 20)) << " MB per matrix, "
             << (3 * sz * 8 / (1 << 20)) << " MB total"
             << (3 * sz * 8 > (16u << 20) ? "  (exceeds the 16 MB L3)" : "  (fits in L3)") << "\n";
        cout << string(66, '-') << "\n";
        cout << left  << setw(26) << "version"
             << right << setw(11) << "time"
             << right << setw(11) << "GFLOP/s"
             << right << setw(12) << "vs i,k,j" << "\n";
        cout << string(66, '-') << "\n";

        double base = 0.0;
        for (int w = 0; w < 5; w++) {
            // the textbook order is O(n^3) with a stride-n inner loop; skip it
            // at the largest size or it dominates the runtime of the example
            if (w == 0 && n > 1024) { cout << left << setw(26) << names[w]
                                           << right << setw(34) << "(skipped — too slow)" << "\n"; continue; }
            double t = run_one(w, A, B, C, n, T);
            if (w == 1) base = t;
            cout << left  << setw(26) << names[w]
                 << right << setw(8)  << fixed << setprecision(0) << t << " ms"
                 << right << setw(11) << fixed << setprecision(1) << gflop / (t / 1000.0)
                 << right << setw(11) << fixed << setprecision(2)
                 << (base > 0 ? base / t : 1.0) << "x" << "\n";
        }
        free(A); free(B); free(C);
    }

    cout << "\nWhat the sweep shows:\n"
            "\n"
            "  The blocked versions hold roughly the same GFLOP/s at every size --\n"
            "  they are compute-bound, and the problem size stops mattering.  The\n"
            "  unblocked i,k,j version is competitive while the matrices fit in L3\n"
            "  and falls off a cliff when they do not, because it re-reads all of B\n"
            "  once per row of A.  So tiling's speedup GROWS with the problem: it is\n"
            "  not a constant-factor tweak, it is a change in how much memory traffic\n"
            "  the algorithm generates.\n"
            "\n"
            "  Blocking ONE dimension is the version to understand first -- it is a\n"
            "  single extra loop on top of the interchanged kernel, and at n=2048 it\n"
            "  is worth about two thirds of the full win.  But look at n=1024, where\n"
            "  it LOSES: the working set only just exceeds L3, and one dimension of\n"
            "  blocking adds loop overhead without cutting enough traffic to pay for\n"
            "  it.  Only blocking all three helps there.  Partial blocking is a\n"
            "  teaching step, not a general shortcut.\n"
            "\n"
            "  Compare loop_tiling.cpp, where tiling a TRANSPOSE is beaten outright\n"
            "  by padding the leading dimension.  A transpose reads every element\n"
            "  once; there is no reuse for a tile to capture, and the only problem\n"
            "  tiling can fix there is one that a better layout fixes more cheaply.\n"
            "  Ask what the reuse factor is before reaching for a tile.\n";

    cout << "\nCore clock at end of run: " << fixed << setprecision(2)
         << core_clock_ghz() << " GHz\n";
    return 0;
}
