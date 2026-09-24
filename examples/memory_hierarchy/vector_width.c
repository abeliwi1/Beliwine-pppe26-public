/* vector_width.c -- does a wider vector make a loop faster?
 *
 * The honest answer is "only while the data is close enough", and this
 * measures where that stops being true.  The same reduction is written three
 * ways -- scalar, 256-bit (AVX2), 512-bit (AVX-512) -- and run over working
 * sets from L1-resident to far past L3.
 *
 * What to expect, and why it matters for a vectorisation lecture:
 *   - in L1 the loop is limited by how many bytes per cycle the core can
 *     load, so doubling the vector width roughly doubles the rate
 *   - out in DRAM the loop is limited by the memory system, which does not
 *     care how wide your registers are, and all three converge
 *
 * A 512-bit load is 64 bytes, which is exactly one cache line on this
 * machine.  One AVX-512 load consumes a whole line; an AVX2 load takes half.
 *
 * Part B sweeps the starting offset so that some vector loads straddle a line
 * boundary, which is the other thing alignment advice is usually about.
 *
 * All three use four accumulators: with one, the loop measures the adder's
 * latency rather than the memory system.
 *
 * Output: CSV on stdout, two blocks tagged by the first column.
 *
 * Build: gcc -O2 -march=native -o vector_width vector_width.c
 */
#include "harness.h"
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

static volatile double sink;

static double sum_scalar(const double *a, size_t n) {
    double s0=0,s1=0,s2=0,s3=0;
    for (size_t i = 0; i + 3 < n; i += 4) {
        s0 += a[i]; s1 += a[i+1]; s2 += a[i+2]; s3 += a[i+3];
    }
    return s0+s1+s2+s3;
}
static double sum_avx2(const double *a, size_t n) {
    __m256d v0=_mm256_setzero_pd(),v1=v0,v2=v0,v3=v0;
    for (size_t i = 0; i + 15 < n; i += 16) {
        v0=_mm256_add_pd(v0,_mm256_loadu_pd(a+i));
        v1=_mm256_add_pd(v1,_mm256_loadu_pd(a+i+4));
        v2=_mm256_add_pd(v2,_mm256_loadu_pd(a+i+8));
        v3=_mm256_add_pd(v3,_mm256_loadu_pd(a+i+12));
    }
    __m256d v = _mm256_add_pd(_mm256_add_pd(v0,v1), _mm256_add_pd(v2,v3));
    double t[4]; _mm256_storeu_pd(t, v);
    return t[0]+t[1]+t[2]+t[3];
}
static double sum_avx512(const double *a, size_t n) {
    __m512d v0=_mm512_setzero_pd(),v1=v0,v2=v0,v3=v0;
    for (size_t i = 0; i + 31 < n; i += 32) {
        v0=_mm512_add_pd(v0,_mm512_loadu_pd(a+i));
        v1=_mm512_add_pd(v1,_mm512_loadu_pd(a+i+8));
        v2=_mm512_add_pd(v2,_mm512_loadu_pd(a+i+16));
        v3=_mm512_add_pd(v3,_mm512_loadu_pd(a+i+24));
    }
    __m512d v = _mm512_add_pd(_mm512_add_pd(v0,v1), _mm512_add_pd(v2,v3));
    return _mm512_reduce_add_pd(v);
}

static double best_gbps(double (*fn)(const double *, size_t),
                        const double *a, size_t n, int reps) {
    double best = 1e30;
    for (int r = 0; r < 3; r++) {
        double t0 = now_s();
        for (int k = 0; k < reps; k++) sink = fn(a, n);
        double dt = (now_s() - t0) / reps;
        if (dt < best) best = dt;
    }
    return n * sizeof(double) / best / 1e9;
}

int main(int argc, char **argv) {
    int cpu = (argc > 1) ? atoi(argv[1]) : 0;
    harness_begin(cpu, "vector_width");

    /* ---- Part A: width against working-set size ---- */
    printf("part,bytes,scalar_GBps,avx2_GBps,avx512_GBps\n");
    for (size_t bytes = 16UL << 10; bytes <= 256UL << 20; bytes *= 2) {
        double *a;
        if (posix_memalign((void **)&a, 64, bytes)) { perror("alloc"); return 1; }
        for (size_t i = 0; i < bytes / sizeof(double); i++) a[i] = 1.0;
        size_t n = bytes / sizeof(double);
        int reps = (int)((2.0 * (1UL << 30)) / bytes); if (reps < 3) reps = 3;
        printf("width,%zu,%.1f,%.1f,%.1f\n", bytes,
               best_gbps(sum_scalar, a, n, reps),
               best_gbps(sum_avx2,   a, n, reps),
               best_gbps(sum_avx512, a, n, reps));
        fflush(stdout);
        free(a);
    }

    /* ---- Part B: what a misaligned start costs, L1-resident ---- */
    printf("part,offset_bytes,avx512_GBps,,\n");
    const size_t SZ = 32UL << 10;
    double *base;
    if (posix_memalign((void **)&base, 4096, SZ + 128)) { perror("alloc"); return 1; }
    for (size_t i = 0; i < (SZ + 128) / sizeof(double); i++) base[i] = 1.0;
    for (int off = 0; off <= 56; off += 8) {
        const double *a = (const double *)((const char *)base + off);
        size_t n = SZ / sizeof(double);
        printf("align,%d,%.1f,,\n", off, best_gbps(sum_avx512, a, n, 20000));
        fflush(stdout);
    }
    free(base);
    return 0;
}
