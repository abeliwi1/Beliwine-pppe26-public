// Loop Interchange Example: Matrix-Vector Multiplication
//
// Computes y = A * x where A is M×N (row-major), x is N×1, y is M×1.
//
// Two loop orderings produce identical results but very different performance:
//
//   j,i order (column access of A):
//     for j: for i: y[i] += A[i][j] * x[j]
//     The inner loop increments i, stepping through column j of A.
//     In row-major storage, A[i][j] = A[i*N + j], so consecutive i values
//     are N doubles apart — stride N*8 bytes per step.  For N=4096 that is
//     32 KB per step: every access is a cache miss.
//
//   i,j order (row access of A):
//     for i: for j: y[i] += A[i][j] * x[j]
//     The inner loop increments j, scanning row i of A sequentially.
//     Stride is 8 bytes — one 64-byte cache line covers 8 doubles, so 7 of
//     every 8 accesses are free.  x[j] is also accessed sequentially and is
//     re-read in full for every row of A, so it is the one thing that has to
//     stay resident: N*8 = 32 KB against a measured 48 KB L1d.  The row of A
//     does NOT need to be retained -- it is read once, in order, and never
//     revisited -- so the question is whether x fits, not whether row+x fits.
//
// Loop interchange transforms j,i -> i,j, converting stride-N column
// access into stride-1 row access.  No algorithmic change; pure reordering.
//
// Build:
//   g++ -O1 -o interchange_O1 loop_interchange.cpp && ./interchange_O1

#include <iostream>
#include <chrono>
#include <climits>
#include <iomanip>
#include <string>
#include <vector>
#include <sched.h>
#include <cstdlib>
#include <cmath>

using namespace std;
using namespace std::chrono;

static const int M    = 4096;   // matrix rows
static const int N    = 4096;   // matrix cols  (A = 4096x4096 x 8B = 128 MB)
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

static const int L1_BYTES   = 49152;    // 48 KB L1d, measured
static const int LINE_BYTES = 64;       // 64-byte line, measured

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
    cout << "  BEFORE  " << left  << setw(42) << a_label
         << right << setw(8) << fixed << setprecision(1) << a_ms << " ms\n";
    cout << "  AFTER   " << left  << setw(42) << b_label
         << right << setw(8) << fixed << setprecision(1) << b_ms  << " ms\n";
    cout << "  Speedup: " << fixed << setprecision(2) << speedup << "x\n";
}

using Matrix = vector<double>;
using Vec    = vector<double>;

inline double& at(Matrix& A, int r, int c) { return A[(size_t)r * N + c]; }
inline double  at(const Matrix& A, int r, int c) { return A[(size_t)r * N + c]; }

// ================================================================
// j,i order — column access of A (cache-unfriendly)
//
// Outer loop: j (column index)
// Inner loop: i (row index) — strides N doubles through A
//
// For each j, x[j] is a scalar (one load, fine).
// A[i][j] with i incrementing: step = N * sizeof(double) = 32 KB.
// Every element of A is a cold cache miss.
// ================================================================

void matvec_ji(const Matrix& A, const Vec& x, Vec& y) {
    fill(y.begin(), y.end(), 0.0);
    for (int j = 0; j < N; j++) {
        double xj = x[j];
        for (int i = 0; i < M; i++)
            y[i] += at(A, i, j) * xj;
    }
}

// ================================================================
// i,j order — row access of A (cache-friendly)
//
// Outer loop: i (row index)
// Inner loop: j (column index) — scans row i of A sequentially
//
// y[i] is a scalar accumulator held in a register for the full
// inner loop.  A[i][j] and x[j] are both stride-1 streams; x stays
// resident (32 KB against a 48 KB L1d) while the row of A streams
// through it.  Note the row does NOT need to be retained -- it is read
// once, in order -- so the relevant question is whether x fits, not
// whether row+x fits.
// ================================================================

void matvec_ij(const Matrix& A, const Vec& x, Vec& y) {
    fill(y.begin(), y.end(), 0.0);
    for (int i = 0; i < M; i++) {
        double acc = 0.0;
        for (int j = 0; j < N; j++)
            acc += at(A, i, j) * x[j];
        y[i] = acc;
    }
}

// ================================================================
// main
// ================================================================

int main() {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();
    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";

    Matrix A(M * N);
    Vec    x(N), y_ji(M), y_ij(M);

    for (int i = 0; i < M; i++)
        for (int j = 0; j < N; j++)
            at(A, i, j) = (i * N + j) * 0.0001;
    for (int j = 0; j < N; j++)
        x[j] = j * 0.001;

    // Correctness check.
    matvec_ji(A, x, y_ji);
    matvec_ij(A, x, y_ij);
    double max_err = 0.0;
    for (int i = 0; i < M; i++)
        max_err = max(max_err, abs(y_ji[i] - y_ij[i]));
    cout << "Correctness check: max_err = " << max_err
         << "  " << (max_err < 1e-6 ? "PASS" : "FAIL") << "\n";

    // Access pattern summary.
    int row_bytes   = N * (int)sizeof(double);
    int x_bytes     = N * (int)sizeof(double);
    int inner_bytes = x_bytes;               // only x must stay resident
    cout << "\nAccess pattern:\n";
    cout << "  A row (one inner loop pass): " << row_bytes/1024 << " KB\n";
    cout << "  x vector:                   " << x_bytes/1024   << " KB\n";
    cout << "  must stay resident (x only): " << inner_bytes/1024
         << " KB  " << (inner_bytes <= L1_BYTES ? "(fits L1)" : "(exceeds L1)")
         << "   [L1d = " << L1_BYTES/1024 << " KB, " << LINE_BYTES
         << "-byte line, measured]\n";
    cout << "  doubles per cache line:     " << LINE_BYTES/(int)sizeof(double)
         << "   (so stride-1 amortises one miss over " << LINE_BYTES/(int)sizeof(double)
         << " elements)\n";
    cout << "  j,i inner stride on A:      " << row_bytes/1024
         << " KB per step  (one miss per element)\n";

    double acc = 0.0;
    auto t_ji = bench([&]{ matvec_ji(A, x, y_ji); return y_ji[0]; }, acc);
    auto t_ij = bench([&]{ matvec_ij(A, x, y_ij); return y_ij[0]; }, acc);

    report("Matrix-vector multiply  M=N=4096  (A = 128 MB)",
           "j,i order  (stride-N column access, cache misses)", t_ji,
           "i,j order  (stride-1 row access,    L1 reuse)",     t_ij);

    cout << "\nCore clock at end of run: " << fixed << setprecision(2)
         << core_clock_ghz() << " GHz"
            "   (if this differs from the figure above, the clock moved\n"
            "                           under the measurement and the times are not comparable)\n";

    // Keep every accumulated kernel result observable, or the compiler is
    // entitled to delete the calls and report a time for work that never ran.
    volatile double keep = acc; (void)keep;

    return 0;
}
