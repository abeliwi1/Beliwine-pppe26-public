// Loop Tiling Example: Matrix Transpose
//
// Naive transpose reads input row-major (stride 1, prefetchable) but writes
// output column-major (stride lda -- a different cache line per write).  The
// hardware prefetcher cannot help with strided writes: each write targets a
// distinct cache line that is evicted before the next write to that line.
//
// Tiling fixes both sides at once.  A B*B input tile is read sequentially;
// the transposed B*B output tile writes to B consecutive elements of B
// different rows -- but those B rows all fall within the same B*B block of
// the output, so they can stay in L1 for the duration of the tile, and each
// output line is written several times before it is evicted.
//
// THAT IS THE TEXTBOOK STORY.  On this machine it is only half the truth, and
// the sweep below is arranged to show which half.
//
// The usual rules for picking B both fail here:
//
//   capacity   2*B^2*8 <= L1 (48 KB)  predicts B <= 55.  Measured, B=48 and
//              B=64 are near the BOTTOM of the table.  Not the constraint.
//
//   "B <= associativity"  predicts B <= 12.  Closer -- the measured optimum
//              is B=16 -- but it does not actually pick the peak, and since
//              associativity varies by machine it does not transfer either.
//
// What actually governs the whole effect is the LEADING DIMENSION, not the
// tile size.  A line's L1 set here is (address / 64) % 64.  Walking down a
// column steps one row = lda*8 bytes = lda/8 lines, so the set index advances
// by (lda/8) % 64 per step, and the column reaches only
//
//     sets_reachable = 64 / gcd((lda/8) % 64, 64)
//
// distinct sets.  At lda=4096 that advance is 0: EVERY element of a column
// lands in ONE set, which has 12 ways, so a 4096-element column evicts itself
// continuously.  Padding lda to 4104 makes the advance 1, the column reaches
// all 64 sets, and the naive transpose gets 7.9x faster with no tiling at all
// -- and lands 1.4x faster than the best tile size ever manages on the
// unpadded array (24 ms against 34 ms).
//
// Tiling is a way to cope with a bad leading dimension.  Fixing the leading
// dimension is a way to not have one.  The second table below is the point of
// this example.
//
// Build:
//   g++ -O1 -o tiling_O1 loop_tiling.cpp && ./tiling_O1

#include <iostream>
#include <chrono>
#include <climits>
#include <iomanip>
#include <string>
#include <vector>
#include <sched.h>
#include <cstdlib>

using namespace std;
using namespace std::chrono;

static const int N    = 4096;   // matrix dimension (128 MB per matrix)
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

static const int L1_BYTES   = 49152;   // 48 KB L1d per core, measured
static const int L1_ASSOC   = 12;      // measured by pointer-chase probe
static const int LINE_BYTES = 64;      // 64-byte line, measured
static const int L1_SETS    = L1_BYTES / (L1_ASSOC * LINE_BYTES);   // 64

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


using Matrix = vector<double>;

// Every kernel takes an explicit leading dimension so that the padding
// experiment changes ONE variable -- the row stride -- and nothing else.
// The logical matrix stays N x N in all cases.

void fill_matrix(Matrix& m, int lda) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            m[(size_t)i * lda + j] = i * N + j;
}

// ================================================================
// Naive transpose -- reads stride-1, writes stride-lda
// ================================================================

void transpose_naive(const Matrix& in, Matrix& out, int lda) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            out[(size_t)j * lda + i] = in[(size_t)i * lda + j];
}

// ================================================================
// Tiled transpose -- B*B blocks
// ================================================================

void transpose_tiled(const Matrix& in, Matrix& out, int lda, int tile) {
    for (int ii = 0; ii < N; ii += tile)
    for (int jj = 0; jj < N; jj += tile) {
        int ilim = min(ii + tile, N);
        int jlim = min(jj + tile, N);
        for (int i = ii; i < ilim; i++)
        for (int j = jj; j < jlim; j++)
            out[(size_t)j * lda + i] = in[(size_t)i * lda + j];
    }
}

// How many distinct L1 sets does one column of a matrix with this leading
// dimension touch?  This is the quantity that decides everything below.
static int sets_reachable(int lda) {
    long row_bytes = (long)lda * sizeof(double);
    if (row_bytes % LINE_BYTES != 0) return -1;     // stride drifts across sets
    long adv = (row_bytes / LINE_BYTES) % L1_SETS;
    long a = adv ? adv : L1_SETS, b = L1_SETS;
    while (b) { long t = a % b; a = b; b = t; }
    return (int)(L1_SETS / a);
}

// ================================================================
// main
// ================================================================

int main() {
    int cpu = pin_to_fast_core(0);
    double ghz = warm_up();
    cout << "Build: " << BUILD_LABEL << "   core clock after warm-up: "
         << fixed << setprecision(2) << ghz << " GHz (cpu" << cpu << ", measured)\n";
    cout << "L1d: " << L1_BYTES/1024 << " KB, " << L1_ASSOC << "-way, "
         << L1_SETS << " sets, " << LINE_BYTES << "-byte line  (all measured)\n";

    double acc = 0.0;

    // ------------------------------------------------------------------
    // Table 1: tile size, at the natural (and pathological) lda = N = 4096
    // ------------------------------------------------------------------
    {
        const int lda = N;
        Matrix in((size_t)N * lda), out((size_t)N * lda);
        fill_matrix(in, lda);

        // Correctness check against a known-good tiling.
        Matrix ref((size_t)N * lda);
        transpose_naive(in, ref, lda);
        transpose_tiled(in, out, lda, 32);
        bool ok = true;
        for (int i = 0; i < N && ok; i++)
            for (int j = 0; j < N && ok; j++)
                if (ref[(size_t)i*lda+j] != out[(size_t)i*lda+j]) ok = false;
        cout << "Correctness (tile=32): " << (ok ? "PASS" : "FAIL") << "\n";

        double t_naive = bench([&]{ transpose_naive(in, out, lda); return out[0]; }, acc);

        cout << "\n" << string(68, '-') << "\n";
        cout << "Table 1  --  tile size, lda = " << lda
             << " (one column reaches " << sets_reachable(lda) << " of "
             << L1_SETS << " L1 sets)\n";
        cout << string(68, '-') << "\n";
        cout << left  << setw(10) << "tile"
             << left  << setw(14) << "work.set"
             << left  << setw(10) << "fits L1"
             << right << setw(10) << "time"
             << right << setw(10) << "speedup" << "\n";
        cout << string(68, '-') << "\n";
        cout << left  << setw(10) << "naive"
             << left  << setw(14) << "-"
             << left  << setw(10) << "-"
             << right << setw(7)  << fixed << setprecision(1) << t_naive << " ms"
             << right << setw(9)  << "1.00x" << "\n";

        for (int tile : {2, 4, 8, 10, 12, 14, 16, 18, 20, 24, 32, 48, 64, 80, 128, 256}) {
            double t = bench([&]{ transpose_tiled(in, out, lda, tile); return out[0]; }, acc);
            int ws = 2 * tile * tile * (int)sizeof(double);
            cout << left  << setw(10) << tile
                 << left  << setw(14) << (to_string(ws / 1024) + " KB")
                 << left  << setw(10) << (ws <= L1_BYTES ? "yes" : "no")
                 << right << setw(7)  << fixed << setprecision(1) << t << " ms"
                 << right << setw(8)  << fixed << setprecision(2) << t_naive / t << "x"
                 << "\n";
        }
    }

    // ------------------------------------------------------------------
    // Table 2: the same transpose, varying ONLY the leading dimension
    //
    // This is the experiment that isolates the cause.  N is still 4096 and
    // the loops are unchanged; only the row stride moves.
    // ------------------------------------------------------------------
    cout << "\n" << string(68, '-') << "\n";
    cout << "Table 2  --  leading dimension, same N=4096 transpose\n";
    cout << string(68, '-') << "\n";
    cout << left  << setw(8)  << "lda"
         << right << setw(10) << "row bytes"
         << right << setw(8)  << "sets"
         << right << setw(11) << "naive"
         << right << setw(11) << "tiled B=16"
         << right << setw(12) << "naive gain" << "\n";
    cout << string(68, '-') << "\n";

    double t_base = 0.0;
    for (int lda : {4096, 4097, 4104, 4112, 4128, 4160, 4224, 4352}) {
        Matrix in((size_t)N * lda), out((size_t)N * lda);
        fill_matrix(in, lda);
        double t_n = bench([&]{ transpose_naive(in, out, lda);     return out[0]; }, acc);
        double t_t = bench([&]{ transpose_tiled(in, out, lda, 16); return out[0]; }, acc);
        if (lda == 4096) t_base = t_n;
        int sr = sets_reachable(lda);
        cout << left  << setw(8)  << lda
             << right << setw(10) << (long)lda * 8
             << right << setw(8)  << (sr < 0 ? string("frac") : to_string(sr))
             << right << setw(8)  << fixed << setprecision(1) << t_n << " ms"
             << right << setw(8)  << fixed << setprecision(1) << t_t << " ms"
             << right << setw(11) << fixed << setprecision(1) << t_base / t_n << "x"
             << "\n";
    }

    cout << "\nReading the two tables:\n"
            "\n"
            "  Table 1 is the conventional result and it is real: at lda=4096 a\n"
            "  column of the output reaches exactly ONE of the 64 L1 sets, that set\n"
            "  has 12 ways, and tiling to B=16 buys about 5.5x.  Note that neither\n"
            "  textbook rule picks B=16.  The capacity rule (2*B^2*8 <= 48 KB) says\n"
            "  B <= 55 and is badly wrong -- B=48 and B=64 are near the bottom.  The\n"
            "  associativity rule says B <= 12, which at least lands in the right\n"
            "  neighbourhood, but the peak itself is empirical: find it by sweeping.\n"
            "  The curve is not even smooth -- B=10 is reproducibly worse than B=8,\n"
            "  because 8 doubles is exactly one 64-byte line and 10 straddles two.\n"
            "\n"
            "  Table 2 is the one that explains Table 1.  Nothing changes but the\n"
            "  row stride.  As soon as a column reaches 16 or more sets, the naive\n"
            "  transpose is ~7.9x faster than it was at lda=4096 -- and 1.4x faster\n"
            "  than the best tile size achieves on the unpadded array.  Sixteen is\n"
            "  not arbitrary: 16 sets x 12 ways is enough to hold the column's\n"
            "  working set, which is the same threshold set_conflict.c finds in\n"
            "  ../memory_hierarchy/ with a completely different kernel.\n"
            "\n"
            "  Note lda=4097 (an ODD number of doubles per row).  The row stride is\n"
            "  no longer a whole number of cache lines, the set index drifts, and it\n"
            "  performs like the well-padded cases.  So the rule is not 'avoid powers\n"
            "  of two' -- 4352 is not a power of two and is still 3.1x off the best.\n"
            "  The rule is how many factors of two the row length contains when it is\n"
            "  measured in cache lines.\n"
            "\n"
            "  The practical order of operations, then, is the reverse of the usual\n"
            "  advice: pad the leading dimension first, and reach for tiling only if\n"
            "  the problem still does not fit.  Two footnotes on this machine:\n"
            "  once the stride is padded, tiling stops helping and starts hurting\n"
            "  (the B=16 column in Table 2 is flat at ~35 ms while the naive column\n"
            "  drops to 24 ms), and huge pages make the unpadded case\n"
            "  about 2x WORSE, because 2 MB pages make the physical addresses as\n"
            "  perfectly congruent as the virtual ones.\n";

    // Keep every accumulated kernel result observable, or the compiler is
    // entitled to delete the calls and report a time for work that never ran.
    volatile double keep = acc; (void)keep;

    return 0;
}
