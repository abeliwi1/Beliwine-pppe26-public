// Multiple Accumulators, Quantified: How Far Does Breaking a Chain Go?
//
// This extends pipeline.cpp.  The transformation -- one accumulator vs several
// independent ones -- is the same one performed by singleAccumulator and
// multipleAccumulators there, on double instead of long long.  Nothing new is
// demonstrated about *why* breaking a dependency chain helps; pipeline.md
// covers that and this file assumes it.
//
// What is new is the sweep.  This file measures K = 1, 2, 4 and 8 accumulators
// so the questions after "is it faster?" can be answered: how many do you
// actually need (K_min = latency / throughput), why does adding more stop
// helping, and what is the ceiling made of (memory bandwidth, 72.5 GB/s here).
//
// Note what this file does NOT demonstrate: multiple accumulators need no
// out-of-order hardware.  The compiler emits the
// independent adds in static order and an in-order pipelined core issues them
// back to back without stalling.  For effects that genuinely require a reorder
// buffer, see ../ILP/fuse_loops_rob.cpp (the window has a finite size) and
// ../ILP/speculative_execution.cpp (executing past an unresolved branch).
//
// A loop-carried dependency stalls any pipelined core, in-order or not:
//
//   Single accumulator — every add reads and writes s:
//     fadd s, s, a[0]   // s not ready until FADD_LATENCY (2) cycles later
//     fadd s, s, a[1]   // must wait for s from the line above
//     fadd s, s, a[2]   // must wait ...
//
//   The ROB sees that fadd[i+1] depends on fadd[i]'s output.  Regardless
//   of how many execution units are available, each add stalls for
//   FADD_LATENCY cycles.  The loop runs at 1 add per FADD_LATENCY cycles.
//
// Independent accumulators break the chain:
//
//   Four accumulators — s0, s1, s2, s3 share no dependencies:
//     fadd s0, s0, a[0]  \
//     fadd s1, s1, a[1]  | ROB issues all four to separate FP units
//     fadd s2, s2, a[2]  | in the same (or back-to-back) cycles
//     fadd s3, s3, a[3]  /
//     fadd s0, s0, a[4]  <- s0's result from 4 adds ago is ready; no stall
//     ...
//
//   With FADD latency = 2 cycles (measured, see below) and 1 FP unit issuing
//   1 add/cycle, 2 accumulators already hides the latency: a new add to s0
//   issues every 2nd iteration, and by then the result from 2 iterations ago
//   is written.  One add issues every cycle; the pipeline is full.  K=4 and
//   K=8 keep helping past that point only because there is more than one FP
//   unit to spread across.
//
//   Beyond that the bottleneck shifts from latency to throughput, and then to
//   memory: at K=8 this benchmark streams 512 MB in 6.9 ms = 72.5 GB/s, which
//   is the actual ceiling, not anything about the dependency chain.
//
// Speedup ceiling from latency alone: FADD_LATENCY x (achieved at K >= latency).
// With FADD_LATENCY = 2 that ceiling is only 2x; the measured 5.39x at K=8 is
// therefore mostly multiple FP units, not latency hiding.
//
// Build:
//   clang++ -O1 -o multiple_accs_O1 multiple_accs.cpp && ./multiple_accs_O1

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

static const int N    = 64 * 1024 * 1024;  // 64M doubles = 512 MB
static const int RUNS = 5;

// Label printed in the results header.  The header used to hardcode
// "-O1 (no SIMD)" no matter how the file was actually built, which made every
// non--O1 run mislabel itself.  There is no predefined macro for the -O level
// (__OPTIMIZE__ is set for -O1 and up without distinguishing them), so this
// reports only what can genuinely be detected.  Pass
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

// Both constants below are MEASURED on this machine, not looked up.  They used
// to be FADD_LATENCY = 3 and CPU_GHZ = 4.0, which were jointly impossible:
// sum_serial is a strict dependency chain and so cannot run faster than one
// FADD latency per element, yet those values computed 2.20 cycles/element --
// below the 3-cycle floor they claimed.  One of them had to be wrong; both
// were.  (The old comment even said "M5 = 4.6 GHz" directly above a 4.0.)
//
// How to re-measure on other hardware: a strictly dependent chain of integer
// ADDs runs at exactly 1 cycle/op on any ARM64 or x86-64 core, so timing one
// gives the core clock directly.  With the clock known, timing a dependent
// FADD chain gives FADD latency in cycles.  On this M5 performance core:
//   integer ADD chain : 0.2248 ns/op -> 4.447 GHz  (stable to +/-0.001 over 4 runs)
//   FADD double chain : 0.4721 ns/op -> 2.10 cycles -> latency 2
// Cross-check: sum_serial then computes 2.45 cycles/element against a 2-cycle
// floor, and each K-accumulator version lands just above its latency/K floor.
//
static const int FADD_LATENCY = 2;

// CPU frequency for CPI calculation (Apple M5 performance core), measured as
// described above.  Re-measure before trusting cycles/elem or CPI on any other
// machine -- every derived column scales linearly with this number.
static const double CPU_GHZ = 4.45;

// Instructions per element in each inner loop (from -O1 assembly):
//   serial: ldr, fadd, subs, b.ne                     = 4.00 / elem
//   2acc:   ldp, fadd×2, add, add, cmp, b.lo  / 2     = 3.50 / elem
//   4acc:   ldp×2, fadd×4, add, cmp, b.lo     / 4     = 2.25 / elem
//   8acc:   ldp×4, fadd×8, add, add, cmp, b.lo / 8    = 2.00 / elem
static const double INSNS[4] = { 4.00, 3.50, 2.25, 2.00 };

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
// Serial accumulation — single loop-carried dependency chain
//
// Every fadd reads s then writes s.  The OOO engine cannot issue
// fadd[i+1] until fadd[i] commits its result: one add per latency cycle.
// ================================================================

double sum_serial(const double* a, int n) {
    double s = 0.0;
    for (int i = 0; i < n; i++)
        s += a[i];
    return s;
}

// ================================================================
// 2 independent accumulators
//
// s0 and s1 carry no dependency between them.  The ROB issues both
// add-chains in parallel.  Latency of each chain is still FADD_LATENCY
// cycles, but the chains run concurrently: effective throughput doubles.
// At FADD_LATENCY = 2 this alone is enough to fill one FP unit.
// ================================================================

double sum_2acc(const double* a, int n) {
    double s0 = 0.0, s1 = 0.0;
    int n2 = n - (n % 2);
    for (int i = 0; i < n2; i += 2) {
        s0 += a[i];
        s1 += a[i + 1];
    }
    for (int i = n2; i < n; i++) s0 += a[i];
    return s0 + s1;
}

// ================================================================
// 4 independent accumulators
//
// With FADD latency = 2 cycles and 1 FP issue port:
//   cycle 0: issue s0 += a[0]
//   cycle 1: issue s1 += a[1]
//   cycle 2: s0 += a[0] result is ready (2-cycle latency elapsed)
//            issue s0 += a[2]   <- no stall
//   ...
// One new fadd issues every cycle and no stall occurs.  Note this means K=2,
// not K=4, is the minimum to saturate a *single* FP unit at 2-cycle latency.
// K=4 and K=8 still measure faster here, which is the evidence that this core
// has more than one FP unit -- latency hiding alone would have stopped at K=2.
// ================================================================

double sum_4acc(const double* a, int n) {
    double s0 = 0.0, s1 = 0.0, s2 = 0.0, s3 = 0.0;
    int n4 = n - (n % 4);
    for (int i = 0; i < n4; i += 4) {
        s0 += a[i];
        s1 += a[i + 1];
        s2 += a[i + 2];
        s3 += a[i + 3];
    }
    for (int i = n4; i < n; i++) s0 += a[i];
    return s0 + s1 + s2 + s3;
}

// ================================================================
// 8 independent accumulators
//
// Saturating every FP unit needs latency * units independent streams.  At the
// measured 2-cycle latency that is 2*units.  The unit count for this core is
// not published -- Apple documents none of this -- so rather than assert one,
// note what the measurement implies: K=8 still beats K=4 (6.9 vs 9.0 ms), so
// 2*units > 4, i.e. this core has at least 3 FP units.  Past K=8 memory
// bandwidth is the floor: 512 MB in 6.9 ms is 72.5 GB/s.
// ================================================================

double sum_8acc(const double* a, int n) {
    double s0=0, s1=0, s2=0, s3=0, s4=0, s5=0, s6=0, s7=0;
    int n8 = n - (n % 8);
    for (int i = 0; i < n8; i += 8) {
        s0 += a[i];
        s1 += a[i + 1];
        s2 += a[i + 2];
        s3 += a[i + 3];
        s4 += a[i + 4];
        s5 += a[i + 5];
        s6 += a[i + 6];
        s7 += a[i + 7];
    }
    for (int i = n8; i < n; i++) s0 += a[i];
    return s0 + s1 + s2 + s3 + s4 + s5 + s6 + s7;
}

// ================================================================
// main
// ================================================================

int main() {
    vector<double> a(N);
    for (int i = 0; i < N; i++)
        a[i] = (i % 1000) * 0.001 + 1.0;   // non-trivial values

    // Correctness check.
    double r1 = sum_serial(a.data(), N);
    double r2 = sum_2acc  (a.data(), N);
    double r4 = sum_4acc  (a.data(), N);
    double r8 = sum_8acc  (a.data(), N);
    double max_err = max({abs(r1-r2), abs(r1-r4), abs(r1-r8)});
    cout << "Correctness check: max_err = " << scientific << max_err
         << "  " << (max_err < 1.0 ? "PASS" : "FAIL") << "\n" << defaultfloat;

    long long mb = (long long)N * sizeof(double) / (1024 * 1024);
    cout << "\nArray:  " << N / (1024*1024) << "M doubles  (" << mb << " MB)\n";
    cout << "FADD latency (Apple M-series): " << FADD_LATENCY
         << " cycles  →  pipeline-full at K >= " << FADD_LATENCY
         << " accumulators\n";

    double acc = 0.0;
    auto t1 = bench([&]{ return sum_serial(a.data(), N); }, acc);
    auto t2 = bench([&]{ return sum_2acc  (a.data(), N); }, acc);
    auto t4 = bench([&]{ return sum_4acc  (a.data(), N); }, acc);
    auto t8 = bench([&]{ return sum_8acc  (a.data(), N); }, acc);

    double times[4] = { t1, t2, t4, t8 };
    auto cpe = [&](int k) {   // cycles per element
        return times[k] * CPU_GHZ * 1e6 / (double)N;
    };
    auto cpi = [&](int k) {   // cycles per instruction
        return cpe(k) / INSNS[k];
    };

    cout << "\n" << string(76, '-') << "\n";
    cout << "Array sum  N=64M doubles  512 MB   " << BUILD_LABEL
         << "   CPU=" << CPU_GHZ << " GHz\n";
    cout << string(76, '-') << "\n";
    cout << left  << setw(24) << "version"
         << right << setw(8)  << "time"
         << right << setw(12) << "cycles/elem"
         << right << setw(7)  << "CPI"
         << right << setw(10) << "speedup" << "\n";
    cout << string(76, '-') << "\n";

    const char* labels[4] = {
        "serial (1 accumulator)",
        "2 accumulators",
        "4 accumulators",
        "8 accumulators"
    };
    for (int k = 0; k < 4; k++) {
        cout << left  << setw(24) << labels[k]
             << right << setw(7)  << fixed << setprecision(1) << times[k] << " ms"
             << right << setw(11) << fixed << setprecision(2) << cpe(k)
             << right << setw(7)  << fixed << setprecision(2) << cpi(k)
             << right << setw(9)  << fixed << setprecision(2)
             << t1 / times[k] << "x\n";
    }

    // Correlate: speedup = (CPI_serial / CPI_fast) × (insns_serial / insns_fast)
    double cpi_ratio  = cpi(0) / cpi(2);
    double insn_ratio = INSNS[0] / INSNS[2];
    cout << fixed << setprecision(2)
         << "\nSpeedup (1→4 acc) = CPI ratio × insn reduction\n"
         << "  = (" << cpi(0) << " / " << cpi(2) << ") × "
         << "(" << INSNS[0] << " / " << INSNS[2] << ")"
         << " = " << cpi_ratio << "x × " << insn_ratio << "x"
         << " = " << cpi_ratio * insn_ratio << "x"
         << "  [measured: " << t1/t4 << "x]\n";

#ifdef __FAST_MATH__
    // Deliberately says only *what was built*, not what happened or why.
    // Relaxed FP semantics change this benchmark's results dramatically, and
    // working out why is the exercise -- see multiple_accs.md.  Do not explain
    // it here.
    cout << "\nBuilt with -ffast-math: relaxed floating-point semantics are in\n"
            "      effect for every FP operation in this file.\n";
#else
    cout << "\nNote: under default FP rules -O2/-O3 do not vectorise the\n"
            "      *reduction*.  FP addition is not associative, so reassociating\n"
            "      a sum is not something the compiler may do uninvited, and the\n"
            "      serial chain survives every level.  They are not inert, though:\n"
            "      they unroll, use wide loads (ldp q, ld4.2d), and pack accumulators\n"
            "      the source already made independent into vector lanes -- sum_8acc\n"
            "      gets 4x fadd.2d at -O2 while serial/2acc/4acc stay scalar.\n"
            "      The compiler exploits the independence you wrote.  It does not\n"
            "      create it.  -O1 shows the pure effect; see multiple_accs.md.\n";
#endif

    volatile double keep_alive = acc;
    (void)keep_alive;

    return 0;
}
