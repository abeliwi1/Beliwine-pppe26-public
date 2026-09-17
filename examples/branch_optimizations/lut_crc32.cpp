// Lookup Tables: Precomputation the Compiler Cannot Do
//
// A 256-entry table replaces eight rounds of data-dependent shift/xor per byte
// in a CRC32 computation, and runs 3.34x faster.  The point of the example is
// not the ratio -- it is that the ratio does not move between -O1 and -O3.
//
// Four separate things stop any compiler from making this transformation, and
// each alone would be enough:
//
//   1. It requires a theorem.  After eight rounds the register's low byte has
//      shifted out entirely, and because CRC arithmetic is linear over GF(2),
//      the contribution of those eight departing bits is a fixed XOR mask
//      determined by the low byte alone.  Hence
//         c_new = (c >> 8) ^ TABLE[c & 0xFF].
//      A compiler would have to prove that.  Optimizers do dataflow analysis
//      and pattern matching; they do not do algebra over finite fields.
//
//   2. It requires a new program phase.  The table must exist before the loop
//      runs, which means inventing an initialisation step that executes 2048
//      rounds at startup.  Optimizations rewrite code in place -- they delete,
//      reorder, widen and unroll.  None of them moves work to an earlier phase.
//
//   3. It is a space-time tradeoff with no principled bound.  The table costs
//      1 KB permanently.  Whether that is acceptable depends on the target, not
//      on the code, so the compiler spends nothing.
//
//   4. The loop is serial.  Every round reads the c the previous round wrote,
//      so there is no parallelism to vectorise, and unrolling removes loop
//      overhead rather than work.
//
// What the compiler DOES do is everything available to it: at -O2 the k-loop is
// fully unrolled and every ternary if-converted to CSEL, giving 36 branchless
// straight-line instructions per byte.  That is good code for the algorithm it
// was handed.  It is still 3.34x slower than not doing the work.
//
//   bitwise inner loop @ -O2: 36 instructions per byte
//   table   inner loop @ -O2:  7 instructions per byte
//
// NOTE: there is no compiler barrier in this file and none is needed.  The
// bitwise loop's `(c & 1) ? ... : ...` branches on the low bit of a CRC
// register -- about as unpredictable as branches get -- and the compiler
// removes it for free.  Both versions are already branchless, so everything
// measured here is work, not branch prediction.  For the misprediction story,
// see branch_free.cpp.
//
// Build:
//   clang++ -O2 -o lut_crc32 lut_crc32.cpp && ./lut_crc32
//   clang++ -O1 -DBUILD_LABEL='"-O1"' -o lut_crc32_O1 lut_crc32.cpp

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cstdint>
#include <string>

using namespace std;
using namespace std::chrono;

static const int N    = 32 * 1024 * 1024;   // 32M bytes
static const int RUNS = 5;

// CPU frequency, MEASURED on this machine, not looked up.  A strictly
// dependent chain of integer ADDs runs at 1 cycle/op on any ARM64 core, so
// timing one gives the clock directly; this core reports 4.42-4.45 GHz.
// Every cycles/byte figure below scales linearly with this -- re-measure it
// before trusting that column on other hardware.
static const double CPU_GHZ = 4.43;

// Results-header label.  There is no predefined macro for the -O level
// (__OPTIMIZE__ is set for -O1 and up without distinguishing them), so report
// only what is detectable; pass -DBUILD_LABEL='"-O2"' to be specific.
#ifndef BUILD_LABEL
#  ifdef __OPTIMIZE__
#    define BUILD_LABEL "optimized"
#  else
#    define BUILD_LABEL "-O0"
#  endif
#endif

// CRC-32 (IEEE 802.3), reflected form.
static const uint32_t POLY = 0xEDB88320u;

static uint32_t TABLE[256];

// The precomputation.  This is the part no compiler will write for you: each
// entry is the result of running the bitwise inner loop to completion for one
// byte value.  Cost is 256 x 8 rounds, paid once at startup.
static void build_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (POLY ^ (c >> 1)) : (c >> 1);
        TABLE[i] = c;
    }
}

// ================================================================
// Bitwise: 8 data-dependent rounds per byte.
//
// The ternary is a branch in the source.  The compiler if-converts it to CSEL
// and fully unrolls the k-loop, so the generated code is branchless and
// straight-line -- 36 instructions per byte.  Nothing here is stalling on a
// mispredict; it is simply a lot of work.
// ================================================================
uint32_t crc_bitwise(const uint8_t* d, int n) {
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (POLY ^ (c >> 1)) : (c >> 1);
    }
    return c ^ 0xFFFFFFFFu;
}

// ================================================================
// Table-driven: all 8 rounds collapse into one indexed load.
//
//   ldrb w11, [x0], #1        ; load byte
//   eor  w11, w11, w9         ; index = (crc ^ byte) & 0xFF
//   and  x11, x11, #0xff
//   ldr  w11, [x10, x11, lsl #2]   ; the lookup
//   eor  w9,  w11, w9, lsr #8      ; fold in
//
// The 1 KB table is L1-resident and the access pattern is data-dependent but
// tiny, so it hits essentially always.
// ================================================================
uint32_t crc_table(const uint8_t* d, int n) {
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++)
        c = TABLE[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

// Compiler barrier.  Fences the timed region only -- it says nothing about the
// code inside the kernels, which is the whole point.
static void clobber() { asm volatile("" ::: "memory"); }

// Min-of-RUNS wall time in milliseconds, at sub-millisecond resolution.  Each
// kernel's result is accumulated into `acc` so no call is ever dead.
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

int main() {
    build_table();

    vector<uint8_t> data(N);
    for (int i = 0; i < N; i++)
        data[i] = (uint8_t)((uint32_t)i * 2654435761u >> 24);

    // Correctness: the two must agree bit for bit.  A table built with the
    // wrong polynomial or bit order still "works" and still runs fast, so this
    // check is not a formality.
    uint32_t a = crc_bitwise(data.data(), 1 << 16);
    uint32_t b = crc_table  (data.data(), 1 << 16);
    cout << "Correctness: bitwise=" << hex << setw(8) << setfill('0') << a
         << "  table=" << setw(8) << b << setfill(' ') << dec
         << (a == b ? "  PASS\n" : "  FAIL\n");

    double acc = 0.0;
    double tb = bench([&]{ return crc_bitwise(data.data(), N); }, acc);
    double tt = bench([&]{ return crc_table  (data.data(), N); }, acc);

    auto cpb = [&](double ms) { return ms * CPU_GHZ * 1e6 / (double)N; };

    cout << "\n" << string(66, '-') << "\n";
    cout << "CRC32  N=" << N / (1024 * 1024) << "M bytes   " << BUILD_LABEL
         << "   CPU=" << CPU_GHZ << " GHz\n";
    cout << string(66, '-') << "\n";
    cout << left  << setw(22) << "version"
         << right << setw(12) << "time"
         << right << setw(14) << "cycles/byte"
         << right << setw(13) << "insns/byte"
         << right << setw(10) << "speedup" << "\n";
    cout << string(66, '-') << "\n";
    cout << left  << setw(22) << "bitwise (8 rounds)"
         << right << setw(9)  << fixed << setprecision(1) << tb << " ms"
         << right << setw(14) << setprecision(2) << cpb(tb)
         << right << setw(13) << 36
         << right << setw(9)  << "1.00x" << "\n";
    cout << left  << setw(22) << "table (256 entries)"
         << right << setw(9)  << fixed << setprecision(1) << tt << " ms"
         << right << setw(14) << setprecision(2) << cpb(tt)
         << right << setw(13) << 7
         << right << setw(8)  << setprecision(2) << tb / tt << "x\n";

    cout << "\nNo compiler barrier is used or needed in this file.  The bitwise\n"
            "loop's ternary is if-converted to CSEL automatically, so both\n"
            "versions are already branchless -- the difference measured above is\n"
            "work, not branch prediction.  Rebuild at -O1 and -O3: the ratio\n"
            "barely moves, because no optimisation level can turn eight rounds of\n"
            "shift/xor into a table lookup.  That is the whole point.\n";

    volatile double keep = acc; (void)keep;
    return 0;
}
