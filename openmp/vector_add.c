#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/*
 *  vector_add.c  --  #pragma omp parallel for
 *
 *  parallel for is OpenMP's work-sharing loop. Iterations must be
 *  INDEPENDENT (no iteration reads a value another writes). By default
 *  the runtime splits the index range into one contiguous chunk per
 *  thread and hands them out up front -- unlike cilk_for, which recursively
 *  halves the range into a tree of spawns and lets idle workers steal
 *  leaves. Static chunking has less overhead when the work per iteration
 *  is uniform; we'll compare it against dynamic/guided scheduling in
 *  later examples (e.g. the prefix-sum scheduling demos) where it isn't.
 *
 *    #pragma omp parallel for
 *    for (i = 0; i < N; i++)
 *        c[i] = a[i] + b[i];       // each i touches a different slot
 *
 *  Compile (clang + libomp on macOS):
 *    clang -Xpreprocessor -fopenmp -O3 \
 *      -I/opt/homebrew/opt/libomp/include \
 *      -L/opt/homebrew/opt/libomp/lib -lomp \
 *      vector_add.c -o vector_add
 *
 *  Usage: ./vector_add [N]     (default N = 100000000)
 */

int main(int argc, char *argv[])
{
    long N = argc > 1 ? atol(argv[1]) : 100000000L;

    double *a = malloc(N * sizeof(double));
    double *b = malloc(N * sizeof(double));
    double *c = malloc(N * sizeof(double));

    #pragma omp parallel for
    for (long i = 0; i < N; i++) {
        a[i] = (double)i;
        b[i] = 2.0 * i;
    }

    #pragma omp parallel for
    for (long i = 0; i < N; i++)
        c[i] = a[i] + b[i];

    printf("c[0] = %.1f   c[%ld] = %.1f\n", c[0], N - 1, c[N - 1]);

    free(a); free(b); free(c);
    return 0;
}
