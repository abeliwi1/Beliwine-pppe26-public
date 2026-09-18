/*
 * activity1_pipeline.cpp
 *
 * Compile and benchmark:
 *   clang++ -std=c++17 -O1 -o activity1 activity1_pipeline.cpp && ./activity1
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <climits>

using namespace std;
using namespace std::chrono;

const int ARRAY_SIZE = 100000000;

static void clobber()
{
    asm volatile("" ::: "memory");
}

// ============================================================
// GIVEN: single accumulator, real RAW hazard
// ============================================================
unsigned long long noTempVars(vector<int> &data)
{
    unsigned long long x = 1;
    for (int i = 0; i < ARRAY_SIZE; i += 4)
    {
        x = x * ((unsigned long long)data[i] | 1ull);     // stall: reads x just written
        x = x * ((unsigned long long)data[i + 1] | 1ull); // stall: depends on line above
        x = x * ((unsigned long long)data[i + 2] | 1ull); // stall: depends on line above
        x = x * ((unsigned long long)data[i + 3] | 1ull); // stall: depends on line above
    }
    return x;
}

// ============================================================
// YOUR TASK: split each loop body into a load phase and a compute
// phase, each internally independent, so the CPU can overlap all
// four multiplies of an unrolled iteration instead of serializing
// them one at a time.
// ============================================================
unsigned long long withTempVars(vector<int> &data)
{
    // Create four independent accumulators. Think about what value to
    // seed them with so the four partial products, multiplied together
    // at the end, equal what the single accumulator produced.
    unsigned long long x0 = 1;
    unsigned long long x1 = 1;
    unsigned long long x2 = 1;
    unsigned long long x3 = 1;

    for (int i = 0; i < ARRAY_SIZE; i += 4)
    {
        // LOAD PHASE: four independent operands from data (remember | 1ull)
        unsigned long long op0 = (unsigned long long)data[i] | 1ull;
        unsigned long long op1 = (unsigned long long)data[i + 1] | 1ull;
        unsigned long long op2 = (unsigned long long)data[i + 2] | 1ull;
        unsigned long long op3 = (unsigned long long)data[i + 3] | 1ull; // tempVars

        // COMPUTE PHASE: four independent multiplies
        x0 = x0 * op0;
        x1 = x1 * op1;
        x2 = x2 * op2;
        x3 = x3 * op3;
    }
    // combine the four partial products into the single return value
    return x0 * x1 * x2 * x3;
}

// ============================================================
// Benchmark harness — do not modify
// ============================================================
template <typename Func>
pair<long long, unsigned long long> benchmark(Func func, vector<int> &data, int runs = 5)
{
    long long minTime = LLONG_MAX;
    unsigned long long result = 0;

    for (int r = 0; r < runs; r++)
    {
        clobber();
        auto start = high_resolution_clock::now();
        clobber();

        // Accumulate, don't overwrite: with `result = func(data)` only the
        // last of the five calls' results is ever read, so the compiler may
        // delete the other four as dead code and the minimum below comes
        // from a run that never happened.
        result += func(data);

        clobber();
        auto end = high_resolution_clock::now();
        clobber();

        long long t = duration_cast<milliseconds>(end - start).count();
        minTime = min(minTime, t);
    }

    volatile unsigned long long dummy = result;
    (void)dummy;

    return {minTime, result};
}

int main()
{
    cout << "=== Activity 1: Independent Accumulators ===" << endl;
    cout << "Array size: " << ARRAY_SIZE << " elements\n"
         << endl;

    vector<int> data(ARRAY_SIZE);
    for (int i = 0; i < ARRAY_SIZE; i++)
    {
        data[i] = (i % 100) + 1;
    }
    clobber();

    // Warmup
    volatile unsigned long long w = noTempVars(data);
    w += withTempVars(data);
    (void)w;

    auto [timeA, resA] = benchmark(noTempVars, data);
    auto [timeB, resB] = benchmark(withTempVars, data);

    cout << "One chain (stalled):        " << setw(5) << timeA << " ms" << endl;
    cout << "Four chains (pipelined):    " << setw(5) << timeB << " ms" << endl;
    cout << "Speedup: " << fixed << setprecision(2)
         << (double)timeA / max(1LL, timeB) << "x" << endl;

    cout << "Same answer? " << (resA == resB ? "YES" : "NO") << endl;

    volatile unsigned long long checksum = resA + resB;
    cout << "(Checksum: " << checksum << ")" << endl;

    return 0;
}
