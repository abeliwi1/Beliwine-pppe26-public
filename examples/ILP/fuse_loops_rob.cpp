// Loop Fusion and the Reorder Window: Why Independent Work Must Be *Near*
//
// This example is about one property of out-of-order hardware: the reorder
// buffer has a finite SIZE, and independent work outside it may as well not
// exist.
//
// The setup poses a question the hardware appears able to answer on its own.
// Two reductions over the same array carry no dependency between them --
// sumsq never reads sum -- so their FADD chains are mutually independent and
// each could fill the other's stall slots.  An out-of-order core issues by
// dependency, not program order.  So why doesn't it just interleave them?
//
// The answer is distance, not permission.  Nothing forbids reordering across a
// loop boundary; a loop's backward branch is among the most predictable
// branches that exist, and the front end sails through it.  But pass 1 runs
// 64M iterations of ~6 instructions -- about 4x10^8 instructions -- before pass
// 2's first add appears.  A reorder buffer holds a few hundred entries.  The
// work the hardware needs is roughly 700,000 windows away.  It never has both
// chains in view, so it cannot discover they are independent.
//
// The ROB is NEAR-SIGHTED, not blocked.  Fusing the loops does not remove a
// barrier; it moves the independent work from 4x10^8 instructions away to ONE
// instruction away, well inside the window.  That is the general shape of the
// technique: a source transformation earns its speedup by bringing independent
// work inside the hardware's reordering horizon.
//
// This example computes two reductions over the same array:
//
//   sum   = Σ x[i]         (one FADD per element, loop-carried dep on sum)
//   sumsq = Σ x[i]²        (one FMUL + one FADD, loop-carried dep on sumsq)
//
// In separate loops the dependency chains run back-to-back:
//
//   [sum loop]    fadd sum sum x[0]   ← stalls here for 2 cycles
//                 fadd sum sum x[1]
//                 ...
//   [sumsq loop]  fmul t   x[0] x[0]  ← waits for sum loop to finish
//                 fadd sumsq sumsq t
//                 ...
//   Total ≈ 2 × N × FADD_latency cycles
//
// In a fused loop both chains run in the SAME loop body.  Because sum and sumsq
// carry no dependency between them, the two FADD chains are independent — and the
// FMUL for sumsq fills the stall slot between consecutive sum updates:
//
//   fadd sum   sum   x[i]     ← dep on previous sum   (2-cycle latency)
//   fmul t     x[i]  x[i]    ← INDEPENDENT; fills 1 stall cycle
//   fadd sumsq sumsq t        ← dep on previous sumsq (2-cycle latency)
//   fadd sum   sum   x[i+1]  ← sum dep resolved while sumsq was computing
//   ...
//
// The dependent sum instructions are "separated" by the independent sumsq work,
// and vice versa.  Each chain runs at the throughput limit of one FADD per cycle
// while the other fills the gap.  Total ≈ N × FADD_latency cycles — a 2× win.
//
// A fused loop with multiple accumulators per chain combines this with the
// multiple-accumulator technique from ../pipeline/multiple_accs.cpp, removing the remaining
// loop-carried bottleneck.
//
// The OOO engine CAN execute independent instructions out of order within its
// reorder buffer (ROB), but it CANNOT look past the end of a completed loop to
// start the next one.  The loop boundary is an ordering barrier.  Fusing the
// loops eliminates that barrier and exposes the inter-chain independence to the
// hardware scheduler.
//
// Build:
//   g++ -O1 -o fuse_O1 fuse_loops_rob.cpp && ./fuse_O1
//
// Use -O1.  Not because -O2 vectorises the reductions -- it does not: FP
// addition is not associative, so reassociating either sum needs -ffast-math,
// and both accumulator chains stay scalar through -O3.  But -O2 is not inert.
// It vectorises the elementwise square in two_passes (fmul.4s -- squaring is
// not a reduction, so no reassociation is involved), and it packs fused_2acc's
// already-independent accumulators into vector lanes (fmul.2s + fadd.2s).
// That speeds the baseline and the best case by different amounts, compressing
// the measured ratio from 3.16x at -O1 to 2.26x at -O2.  -O1 shows the ILP
// effect on its own; -O2 shows what the compiler adds on top of it.

#include <iostream>
#include <chrono>
#include <climits>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;
using namespace std::chrono;

static const int N    = 64 * 1024 * 1024;  // 64M floats = 256 MB
static const int RUNS = 5;

// Results-header label.  This used to hardcode "-O1 (no SIMD)" no matter how
// the file was built, so every non--O1 run mislabelled itself.  There is no
// predefined macro for the -O level (__OPTIMIZE__ is set for -O1 and up
// without distinguishing them), so report only what is detectable; pass
// -DBUILD_LABEL='"-O2"' to be specific.
#ifndef BUILD_LABEL
#  ifdef __FAST_MATH__
#    define BUILD_LABEL "-ffast-math (relaxed FP)"
#  elif defined(__OPTIMIZE__)
#    define BUILD_LABEL "optimized, default FP"
#  else
#    define BUILD_LABEL "-O0, default FP"
#  endif
#endif

// MEASURED on this machine, not looked up.  These were FADD_LATENCY = 3 and
// CPU_GHZ = 4.0, which were jointly wrong in compensating directions -- the
// old "2 loops x 3 cycles = 6" model appeared to match a measured 5.37 only
// because both constants were off.  Method: a strictly dependent chain of
// integer ADDs runs at 1 cycle/op on any ARM64 core, so timing one gives the
// clock; a dependent FADD chain then gives FP latency in cycles.  On this M5
// performance core, over 3 runs each:
//   clock             : 4.431-4.448 GHz
//   FADD float chain  : 0.4702 ns/op -> 2.08 cycles -> latency 2
//   FMUL float chain  : 0.6787 ns/op -> 3.01 cycles -> latency 3
//
// Note FMUL (3) is SLOWER than FADD (2), but it does not lengthen either
// chain: x[i]*x[i] depends only on the load, so the multiply sits *off* the
// loop-carried path and feeds the sumsq add from the side.  Both chains are
// therefore 2 cycles deep per element, not 3 or 5.
//
static const int FADD_LATENCY = 2;
static const int FMUL_LATENCY = 3;   // off-chain; see above

// CPU frequency (Apple M5 performance core), measured as described above.
// Every cycles/elem and CPI figure scales linearly with this -- re-measure
// before trusting those columns on other hardware.
static const double CPU_GHZ = 4.44;

// Instructions per element (approximate, from -O1 ARM64 assembly):
//   two_passes:    ldr, fadd, loop / 1   → 3 per chain, ~6 total / elem
//   fused_1acc:    ldr, fadd, fmul, fadd, loop / 1   → ~6 / elem
//   fused_2acc:    ldp, fadd×2, fmul×2, fadd×2, loop / 2 → ~5 / elem
static const double INSNS[3] = { 6.0, 6.0, 5.0 };

// Compiler barrier.  This fences the timed region only -- it says nothing
// about the code inside the kernels, which is the whole point.
static void clobber() { asm volatile("" ::: "memory"); }

// Min-of-RUNS wall time in milliseconds, at sub-millisecond resolution.
//
// The lambda returns its kernel's result and we accumulate every one into
// `acc`, so no call is ever dead.  An earlier version instead applied a
// per-call sink() fence to the returned value.  That worked, but a fence
// is a blunt instrument: accumulating keeps all RUNS calls observable
// without telling the optimizer anything about the code being timed.
// (Dropping both -- `f()` with the result discarded -- would let the
// compiler delete the calls outright and report a time for work that
// never ran.  See ../pipeline/pipeline.dce.md.)
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

// ================================================================
// Two separate loops — sequential, no instruction-level overlap
//
// The sum loop carries a dependency on s through every iteration:
//   fadd s, s, x[0]    ← issues; s not ready for 2 cycles
//   fadd s, s, x[1]    ← must wait; stalls 2 cycles each iteration
//
// The sumsq loop cannot begin until the sum loop fully completes.
// The OOO engine's reorder buffer cannot look past the barrier
// between the two loops.  Total ≈ 2 × N × FADD_latency cycles.
// ================================================================

void two_passes(const float* x, int n, float& s_out, float& ss_out) {
    float s = 0.0f;
    for (int i = 0; i < n; i++)
        s += x[i];

    float ss = 0.0f;
    for (int i = 0; i < n; i++)
        ss += x[i] * x[i];

    s_out = s;  ss_out = ss;
}

// ================================================================
// Fused loop — one pass, both chains interleaved
//
// s and ss carry no dependency between them.  The instruction sequence
// for one iteration at -O1 looks like:
//
//   ldr  s2, [x0]            ; load x[i]
//   fadd s0, s0, s2          ; sum  += x[i]    ← dep on previous s0
//   fmul s3, s2,  s2         ; x[i]*x[i]       ← INDEPENDENT (fills gap)
//   fadd s1, s1,  s3         ; sumsq += x[i]²  ← dep on previous s1
//
// The fmul fills 1 of the 3 stall cycles between s0[i] and s0[i+1].
// The sumsq fadd fills 1 more.  By the time the loop returns to
// update s0 again, 2 independent instructions have issued — reducing
// effective stall from 2 cycles to ~0 cycles on an OOO core.
// Total ≈ N × FADD_latency cycles — ~2× faster than two_passes.
// ================================================================

void fused_1acc(const float* x, int n, float& s_out, float& ss_out) {
    float s = 0.0f, ss = 0.0f;
    for (int i = 0; i < n; i++) {
        s  += x[i];
        ss += x[i] * x[i];
    }
    s_out = s;  ss_out = ss;
}

// ================================================================
// Fused loop with 2 accumulators per chain
//
// Combines instruction scheduling (fusing) with the multiple-accumulator
// technique from ../pipeline/multiple_accs.cpp.  Two accumulators per chain double the
// number of independent instructions between each loop-carried fadd,
// matching FADD_latency = 3 exactly and eliminating all stalls.
//
//   fadd s0, s0, x[i]          \
//   fadd s1, s1, x[i+1]        |  two independent sum updates
//   fmul t0, x[i],   x[i]      |  four independent ops between s0[i]
//   fmul t1, x[i+1], x[i+1]   |  and s0[i+2]: latency fully hidden
//   fadd q0, q0, t0            |
//   fadd q1, q1, t1            /
// ================================================================

void fused_2acc(const float* x, int n, float& s_out, float& ss_out) {
    float s0 = 0.0f, s1 = 0.0f;
    float q0 = 0.0f, q1 = 0.0f;
    int n2 = n & ~1;
    for (int i = 0; i < n2; i += 2) {
        s0 += x[i];
        s1 += x[i+1];
        q0 += x[i]   * x[i];
        q1 += x[i+1] * x[i+1];
    }
    if (n & 1) { s0 += x[n-1]; q0 += x[n-1] * x[n-1]; }
    s_out  = s0 + s1;
    ss_out = q0 + q1;
}

// ================================================================
// main
// ================================================================

int main() {
    vector<float> x(N);
    for (int i = 0; i < N; i++)
        x[i] = (i % 1000) * 0.001f + 0.5f;

    float s1, ss1, s2, ss2, s3, ss3;

    // Correctness check on a small slice (float precision is lost at large N:
    // when sum ≈ 64M the float ULP is ~8, swamping individual additions of ~1.0).
    static const int NC = 10000;
    float cs1, css1, cs2, css2, cs3, css3;
    two_passes (x.data(), NC, cs1, css1);
    fused_1acc (x.data(), NC, cs2, css2);
    fused_2acc (x.data(), NC, cs3, css3);
    float es  = max(abs(cs1-cs2),  abs(cs1-cs3));
    float eq  = max(abs(css1-css2), abs(css1-css3));
    float res = abs(cs1)  > 0 ? es  / abs(cs1)  : es;
    float req = abs(css1) > 0 ? eq  / abs(css1) : eq;
    cout << "Correctness (N=" << NC << "): rel_sum_err=" << res << "  rel_sumsq_err=" << req
         << "  " << (res < 0.001f && req < 0.001f ? "PASS" : "FAIL") << "\n";

    long long mb = (long long)N * sizeof(float) / (1024*1024);
    cout << "\nArray: " << N/(1024*1024) << "M floats  (" << mb << " MB)\n";
    cout << "FADD latency (Apple M-series): " << FADD_LATENCY
         << " cycles  →  both chains fill each other's stall slots when fused\n";

    double acc = 0.0;
    auto t1 = bench([&]{ two_passes(x.data(), N, s1, ss1); return s1 + ss1; }, acc);
    auto t2 = bench([&]{ fused_1acc(x.data(), N, s2, ss2); return s2 + ss2; }, acc);
    auto t3 = bench([&]{ fused_2acc(x.data(), N, s3, ss3); return s3 + ss3; }, acc);

    double times[3] = { t1, t2, t3 };
    auto cpe = [&](int k) {
        return times[k] * CPU_GHZ * 1e6 / (double)N;
    };
    auto cpi = [&](int k) {
        return cpe(k) / INSNS[k];
    };

    cout << "\n" << string(76, '-') << "\n";
    cout << "sum + sumsq   N=64M floats  256 MB   " << BUILD_LABEL
         << "   CPU=" << CPU_GHZ << " GHz\n";
    cout << string(76, '-') << "\n";
    cout << left  << setw(22) << "version"
         << right << setw(8)  << "time"
         << right << setw(12) << "cycles/elem"
         << right << setw(7)  << "CPI"
         << right << setw(10) << "speedup" << "\n";
    cout << string(76, '-') << "\n";

    const char* labels[3] = {
        "two_passes",
        "fused_1acc",
        "fused_2acc"
    };
    for (int k = 0; k < 3; k++) {
        cout << left  << setw(22) << labels[k]
             << right << setw(7)  << fixed << setprecision(1) << times[k] << " ms"
             << right << setw(11) << fixed << setprecision(2) << cpe(k)
             << right << setw(7)  << fixed << setprecision(2) << cpi(k)
             << right << setw(9)  << fixed << setprecision(2)
             << t1 / times[k] << "x\n";
    }

    cout << "\nTheoretical cycles per element (floor; -O1 adds ~1/loop of overhead):\n"
         << "  two_passes: 2 loops × " << FADD_LATENCY << " cycles/elem = "
         << 2*FADD_LATENCY << " cycles  (pass 2 is ~700,000 ROB windows away)\n"
         << "  fused_1acc: 1 pass, both chains in the window ≈ "
         << FADD_LATENCY << " cycles  (each chain fills the other's stalls)\n"
         << "  fused_2acc: 2 accumulators × 2 chains ≈ "
         << FADD_LATENCY/2 << " cycle\n";

    cout << "\nNote: -O2/-O3 never vectorise the *reductions* -- FP addition is not\n"
            "      associative, so reassociating a sum needs -ffast-math.  Both\n"
            "      accumulator chains stay scalar through -O3.  What -O2 does add:\n"
            "      fmul.4s for two_passes' elementwise square, and vector lanes for\n"
            "      fused_2acc's already-independent accumulators.  Speeding baseline\n"
            "      and best case unequally compresses 3.16x (-O1) to 2.26x (-O2).\n";

    volatile double keep_alive = acc;
    (void)keep_alive;

    return 0;
}
