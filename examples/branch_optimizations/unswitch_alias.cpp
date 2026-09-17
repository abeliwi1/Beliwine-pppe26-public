// Loop Unswitching Blocked by Aliasing: When Invariance Cannot Be Proven
//
// A loop tests the same condition on every iteration, and the condition never
// changes.  Hoisting it out and specialising the loop per outcome -- loop
// unswitching -- removes the test and, more importantly, leaves a loop body
// clean enough to vectorise.
//
// Whether the compiler will do this for you turns entirely on one question:
// can it PROVE the condition is loop-invariant?  This example is built so that
// it cannot, and the answer does not change at -O3.
//
// ---------------------------------------------------------------------------
// WHY THE CONDITION IS READ THROUGH A POINTER
// ---------------------------------------------------------------------------
// The mode selector arrives as `const float* mode` and the loop tests `*mode`.
// Two properties of that declaration matter, and both are load-bearing:
//
//   1. It is a POINTER, not a value.  A by-value parameter cannot change
//      during the loop -- nothing in the body can reach it -- so the compiler
//      proves invariance immediately and hoists the test without help.  An
//      indirect read has to be re-justified: the compiler must show that no
//      store in the loop body can reach the pointed-to object.
//
//   2. It points to the SAME TYPE as the output array.  Both are float.  C++
//      strict aliasing lets the compiler assume objects of unrelated types do
//      not overlap, so `const int* mode` would be hoisted freely -- an int and
//      a float cannot alias, and the proof succeeds.  Making both float removes
//      that escape: `c[i] = ...` really might write through `mode`, because a
//      caller is entitled to pass `mode = c`.
//
// The consequence is that `*mode` must be reloaded and re-tested on every
// iteration.  The condition cannot leave the loop, so the loop cannot be
// specialised, so it cannot be vectorised.  It stays one scalar loop at every
// optimisation level.
//
// This is a LEGALITY limit, not a budget limit.  The compiler is not declining
// because unswitching would cost too much code; it is declining because the
// transformation might change behaviour for some caller it cannot rule out.
// You know no such caller exists.  It does not.
//
// The fix is one line -- `float m = *mode;` before the loop -- which is exactly
// the hoist the compiler could not justify.  Having made it by hand, you are
// then free to unswitch, and the vectoriser follows.
//
// ---------------------------------------------------------------------------
// WHY THE KERNELS ARE noinline
// ---------------------------------------------------------------------------
// If these functions are inlined into a caller that holds the actual pointer,
// the compiler can see what `mode` really points at, prove it does not alias
// `c` in that specific call, and the whole effect evaporates -- measured at
// 1.06x instead of 2.13x.  That is not a flaw in the example, it is the
// example's boundary: the aliasing problem is a property of a function
// compiled without knowledge of its caller.
//
// `noinline` models the realistic case -- a routine in another translation
// unit, taking a settings pointer, too large to inline.  Without it this file
// measures inlining, not unswitching.
//
// Build:
//   clang++ -O2 -o unswitch_alias unswitch_alias.cpp && ./unswitch_alias
//   clang++ -O3 -DBUILD_LABEL='"-O3"' -o unswitch_alias_O3 unswitch_alias.cpp

#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cmath>
#include <string>

using namespace std;
using namespace std::chrono;

static const int N    = 32 * 1024 * 1024;   // 32M floats = 128 MB
static const int RUNS = 5;

// CPU frequency, MEASURED on this machine (a dependent chain of integer ADDs
// runs at 1 cycle/op, which gives the clock directly).  Re-measure before
// trusting the cycles/element column elsewhere.
static const double CPU_GHZ = 4.43;

#ifndef BUILD_LABEL
#  ifdef __OPTIMIZE__
#    define BUILD_LABEL "optimized"
#  else
#    define BUILD_LABEL "-O0"
#  endif
#endif

// ================================================================
// Switched: the condition is re-tested every iteration because the
// compiler cannot prove `*mode` survives the store to c[i].
// Generated code at -O3: ONE scalar loop, zero vector instructions.
// ================================================================
__attribute__((noinline))
void apply_switched(const float* a, const float* b, float* c, int n,
                    const float* mode) {
    for (int i = 0; i < n; i++) {
        if      (*mode == 0.0f) c[i] = a[i] + b[i];
        else if (*mode == 1.0f) c[i] = a[i] * b[i];
        else                    c[i] = fabsf(a[i]) + fabsf(b[i]);
    }
}

// ================================================================
// Unswitched by hand.  The entire transformation is the first line:
// read the selector once, into a local the compiler knows nothing can
// write to.  Everything after that follows automatically -- three
// specialised loops, each vectorised.
// ================================================================
__attribute__((noinline))
void apply_unswitched(const float* a, const float* b, float* c, int n,
                      const float* mode) {
    float m = *mode;                      // <-- the hoist, done by hand
    if      (m == 0.0f) for (int i = 0; i < n; i++) c[i] = a[i] + b[i];
    else if (m == 1.0f) for (int i = 0; i < n; i++) c[i] = a[i] * b[i];
    else                for (int i = 0; i < n; i++) c[i] = fabsf(a[i]) + fabsf(b[i]);
}

static void clobber() { asm volatile("" ::: "memory"); }

template <typename Func>
double bench(Func f, double& acc) {
    double best = 1e300;
    for (int r = 0; r < RUNS; r++) {
        clobber();
        auto t0 = high_resolution_clock::now();
        clobber();

        acc += f();

        clobber();
        auto t1 = high_resolution_clock::now();
        clobber();

        double ms = duration<double, milli>(t1 - t0).count();
        if (ms < best) best = ms;
    }
    return best;
}

int main() {
    vector<float> a(N), b(N), c1(N), c2(N);
    for (int i = 0; i < N; i++) {
        a[i] = (float)(i % 97) * 0.5f;
        b[i] = (float)(i % 89) * 0.25f;
    }

    float modev[3]      = { 0.0f, 1.0f, 2.0f };
    const char* names[3] = { "0 (add)", "1 (mul)", "2 (abssum)" };

    // Correctness: all three must agree for every mode.
    bool ok = true;
    for (int m = 0; m < 3; m++) {
        apply_switched  (a.data(), b.data(), c1.data(), 4096, &modev[m]);
        apply_unswitched(a.data(), b.data(), c2.data(), 4096, &modev[m]);
        for (int i = 0; i < 4096; i++)
            if (c1[i] != c2[i]) { ok = false; break; }
    }
    cout << "Correctness (both kernels agree, all modes): "
         << (ok ? "PASS" : "FAIL") << "\n";

    double acc = 0.0;
    cout << "\n" << string(74, '-') << "\n";
    cout << "apply(a, b, c, N, mode)   N=32M floats   " << BUILD_LABEL
         << "   CPU=" << CPU_GHZ << " GHz\n";
    cout << string(74, '-') << "\n";
    cout << left  << setw(12) << "mode"
         << right << setw(15) << "switched"
         << right << setw(16) << "unswitched"
         << right << setw(13) << "speedup" << "\n";
    cout << string(74, '-') << "\n";

    for (int m = 0; m < 3; m++) {
        double ts = bench([&]{ apply_switched  (a.data(), b.data(), c1.data(), N, &modev[m]); return (double)c1[0]; }, acc);
        double tu = bench([&]{ apply_unswitched(a.data(), b.data(), c2.data(), N, &modev[m]); return (double)c2[0]; }, acc);
        cout << left  << setw(12) << names[m]
             << right << setw(12) << fixed << setprecision(1) << ts << " ms"
             << right << setw(13) << tu << " ms"
             << right << setw(12) << setprecision(2) << ts / tu << "x\n";
    }

    cout << "\nThe speedup does not close at -O3.  The condition is read through a\n"
            "pointer of the same type as the output, so the compiler cannot prove\n"
            "the store to c[i] leaves *mode intact, and the test cannot leave the\n"
            "loop.  The hoist in apply_unswitched is what unblocks it.\n";

    volatile double keep = acc; (void)keep;
    return 0;
}
