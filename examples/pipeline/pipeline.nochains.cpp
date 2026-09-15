/*
 * pipeline.nochains.cpp
 *
 * A/B variant of pipeline.cpp. Everything is identical except how
 * independentChains' loop body is *written*: here the four chains are
 * interleaved by operation (x1's multiply, x2's, x3's, x4's, then the next
 * multiply for all four) instead of grouped by chain (all four of x1's
 * multiplies, then all four of x2's, ...).
 *
 * The name is slightly loose: the four dependency chains still exist and
 * are just as long. What's gone is *successive-line* dependency -- no
 * statement here reads a value the line immediately above it wrote. Each
 * statement's operand was produced four lines earlier instead of one.
 * Same instructions, same dependency graph, same answer as pipeline.cpp --
 * only the source order differs.
 *
 * Point of the experiment: on an out-of-order machine, does that source
 * ordering matter? Two separate schedulers sit between what you type and
 * what executes -- LLVM's own instruction scheduler (which reorders
 * independent operations at compile time) and the CPU's out-of-order
 * execution engine (which issues by operand readiness at runtime, not
 * fetch order). It turns out to matter a lot at -O0 and not at all at -O1;
 * see pipeline.nochains.md for the measurements and why.
 *
 * Compile and benchmark:
 *   clang++ -std=c++17 -O0 -o pipeline.nochains pipeline.nochains.cpp && ./pipeline.nochains
 *   clang++ -std=c++17 -O1 -o pipeline.nochains pipeline.nochains.cpp && ./pipeline.nochains
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

using namespace std;
using namespace std::chrono;

const int ARRAY_SIZE = 100000000;

// Memory barrier
static void clobber() {
    asm volatile("" ::: "memory");
}

// Build with -DPIPELINE_DEBUG to force noinline on functions marked
// DEBUG_NOINLINE below, so a debugger lands on a real, callable,
// out-of-line copy instead of whatever gets inlined into main -- see the
// comment on independentChains. Without that flag (the normal/measurement
// build), this expands to nothing and the compiler is free to inline as
// it likes, so timing numbers aren't affected by debug-only scaffolding.
#ifdef PIPELINE_DEBUG
#define DEBUG_NOINLINE __attribute__((noinline))
#else
#define DEBUG_NOINLINE
#endif

// ============================================================
// EXAMPLE 1: Dependent Chain vs Independent Operations
// ============================================================

// SLOW: Each operation depends on the previous one
// Pipeline STALLS waiting for each result
//
// Four dependent multiplies per element. Multiply is associative, which is
// what makes the independentChains split below a LEGAL transformation --
// both functions return the same value. (An earlier version of this example
// mixed * with ^ and >>, which stalls just as well but is NOT associative,
// so the "fast" version computed a different answer and wasn't a
// transformation of this one at all.)
//
// Unsigned: 100M multiplies overflow immediately, and signed overflow is
// undefined behavior. The | 1 keeps every factor odd -- multiplying by even
// values drives the low bits to zero and collapses x within 64 elements.
long long dependentChain(vector<int>& data) {
    unsigned long long x = 1;

    for (int i = 0; i < ARRAY_SIZE; i++) {
        unsigned long long v = (unsigned long long)data[i] | 1ull;
        // DEPENDENCY CHAIN: each line needs previous result
        x = x * v;
        x = x * (v + 2);
        x = x * (v + 4);
        x = x * (v + 6);
    }

    return (long long)x;
}

// FAST: Four independent chains processed simultaneously
// Pipeline stays FULL - CPU executes 4 operations in parallel
//
// Written interleaved BY OPERATION rather than grouped by chain: read top
// to bottom and no line depends on the line above it. Compare against
// pipeline.cpp's version, where each chain's four multiplies sit together
// and every line *does* read what the previous line wrote.
//
// DEBUG_NOINLINE (see above): with -DPIPELINE_DEBUG, forces a real,
// callable out-of-line copy so the debugger lands on the schedule LLVM
// chooses in isolation rather than the more conservative schedule it picks
// once this loop is inlined into main under main's register pressure.
// Without that flag, this is a no-op -- normal builds let the compiler
// inline as usual, so measured timings aren't affected.
DEBUG_NOINLINE
long long independentChains(vector<int>& data) {
    // Four SEPARATE accumulators - no dependencies between them.
    // All start at 1: this is a product, so 1 is the identity.
    unsigned long long x1 = 1, x2 = 1, x3 = 1, x4 = 1;

    for (int i = 0; i < ARRAY_SIZE; i += 4) {
        unsigned long long a = (unsigned long long)data[i]     | 1ull;
        unsigned long long b = (unsigned long long)data[i + 1] | 1ull;
        unsigned long long c = (unsigned long long)data[i + 2] | 1ull;
        unsigned long long d = (unsigned long long)data[i + 3] | 1ull;

        // Multiply 1 for all four chains -- none of these four lines
        // depends on any other
        x1 = x1 * a;
        x2 = x2 * b;
        x3 = x3 * c;
        x4 = x4 * d;

        // Multiply 2 for all four chains -- each reads a value written
        // four lines up, not one
        x1 = x1 * (a + 2);
        x2 = x2 * (b + 2);
        x3 = x3 * (c + 2);
        x4 = x4 * (d + 2);

        // Multiply 3 for all four chains
        x1 = x1 * (a + 4);
        x2 = x2 * (b + 4);
        x3 = x3 * (c + 4);
        x4 = x4 * (d + 4);

        // Multiply 4 for all four chains
        x1 = x1 * (a + 6);
        x2 = x2 * (b + 6);
        x3 = x3 * (c + 6);
        x4 = x4 * (d + 6);
    }

    // Recombine by multiply -- the same operator the lanes used, which is
    // why this equals dependentChain(data) exactly.
    return (long long)(x1 * x2 * x3 * x4);
}

// ============================================================
// EXAMPLE 2: Single Accumulator vs Multiple Accumulators
// ============================================================

// SLOW: Single accumulator - dependency every iteration
long long singleAccumulator(vector<int>& data) {
    long long sum = 0;
    
    for (int i = 0; i < ARRAY_SIZE; i++) {
        sum += data[i];  // Must wait for previous sum
    }
    
    return sum;
}

// FAST: Multiple accumulators - can add in parallel
long long multipleAccumulators(vector<int>& data) {
    long long sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
    long long sum5 = 0, sum6 = 0, sum7 = 0, sum8 = 0;
    
    for (int i = 0; i < ARRAY_SIZE; i += 8) {
        sum1 += data[i];      // These 8 additions are
        sum2 += data[i + 1];  // INDEPENDENT of each other
        sum3 += data[i + 2];  // CPU can execute them
        sum4 += data[i + 3];  // simultaneously in the
        sum5 += data[i + 4];  // pipeline!
        sum6 += data[i + 5];
        sum7 += data[i + 6];
        sum8 += data[i + 7];
    }
    
    return sum1 + sum2 + sum3 + sum4 + sum5 + sum6 + sum7 + sum8;
}


// Benchmark helper
template <typename Func>
pair<long long, long long> benchmark(Func func, vector<int>& data, int runs = 5) {
    long long minTime = LLONG_MAX;
    long long result = 0;
    
    for (int r = 0; r < runs; r++) {
        clobber();
        auto start = high_resolution_clock::now();
        clobber();
        
        // Accumulate rather than overwrite: with `result = func(data)` only
        // the last of the `runs` calls is ever read, so the compiler is free
        // to delete the other four as dead code and the minimum below latches
        // onto a run that never happened.
        result += func(data);

        clobber();
        auto end = high_resolution_clock::now();
        clobber();
        
        long long time = duration_cast<milliseconds>(end - start).count();
        minTime = min(minTime, time);
    }
    
    // Prevent optimization
    volatile long long dummy = result;
    (void)dummy;
    
    return {minTime, result};
}

int main() {
    cout << "Array size: " << ARRAY_SIZE << " elements" << endl;
    cout << "(independentChains interleaved by operation -- "
         << "no successive-line dependencies)\n" << endl;

    vector<int> data(ARRAY_SIZE);
    for (int i = 0; i < ARRAY_SIZE; i++) {
        data[i] = (i % 100) + 1;
    }
    clobber();
    
    // Warmup
    volatile long long warmup = dependentChain(data);
    warmup += independentChains(data);
    (void)warmup;
    
    cout << "TEST 1: Dependent Chain vs Independent Chains" << endl;

    auto [time1a, res1a] = benchmark(dependentChain, data);
    auto [time1b, res1b] = benchmark(independentChains, data);

    cout << "Dependent chain:     " << setw(5) << time1a << " ms" << endl;
    cout << "Independent chains:  " << setw(5) << time1b << " ms" << endl;
    cout << "Speedup: " << fixed << setprecision(2)
         << (double)time1a / max(1LL, time1b) << "x" << endl;
    cout << "Same answer? " << (res1a == res1b ? "YES" : "NO")
         << "  (splitting a product into independent lanes is legal)" << endl;

    cout << "\nTEST 2: Single vs Multiple Accumulators" << endl;

    auto [time2a, res2a] = benchmark(singleAccumulator, data);
    auto [time2b, res2b] = benchmark(multipleAccumulators, data);

    cout << "Single accumulator:    " << setw(5) << time2a << " ms" << endl;
    cout << "Multiple accumulators: " << setw(5) << time2b << " ms" << endl;
    cout << "Speedup: " << fixed << setprecision(2)
         << (double)time2a / max(1LL, time2b) << "x" << endl;
    cout << "Same answer? " << (res2a == res2b ? "YES" : "NO")
         << "  (splitting a sum into independent lanes is legal)" << endl;

    // Prevent all results from being optimized away
    volatile long long final_check = res1a + res1b + res2a + res2b;
    cout << "(Checksum: " << final_check << ")\n" << endl;

    return 0;
}