#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/*
 *  reduction.c  --  #pragma omp parallel for reduction(op:var)
 *
 *  A plain `sum += a[i]` inside a parallel for is a RACE: many threads
 *  update the same location. The reduction clause fixes this without an
 *  explicit lock. Each thread gets a private copy of `sum`, initialised to
 *  the identity element for `+` (0.0); at the end of the loop the runtime
 *  combines all the private copies into the original variable.
 *
 *  This is OpenMP's counterpart to a Cilk reducer/hyperobject, but it's
 *  built into the pragma -- no identity/reduce callback functions to
 *  write. The tradeoff is less flexibility: OpenMP predefines a fixed set
 *  of reduction operators (+, -, *, &, |, ^, &&, ||, min, max, ...), while
 *  a Cilk reducer can combine any user-defined type with any function.
 *
 *  Compile (clang + libomp on macOS):
 *    clang -Xpreprocessor -fopenmp -O3 \
 *      -I/opt/homebrew/opt/libomp/include \
 *      -L/opt/homebrew/opt/libomp/lib -lomp \
 *      reduction.c -o reduction
 *
 *  Usage: ./reduction [N]    (default N = 100000000)
 */

int main(int argc, char *argv[])
{
    long N = argc > 1 ? atol(argv[1]) : 100000000L;

    double *a = malloc(N * sizeof(double));
    #pragma omp parallel for
    for (long i = 0; i < N; i++)
        a[i] = 1.0 / (i + 1);        /* harmonic series terms */

    double sum = 0.0;
    #pragma omp parallel for reduction(+:sum)
    for (long i = 0; i < N; i++)
        sum += a[i];                 /* safe: each thread updates its own private copy */

    printf("sum of 1/i for i=1..%ld = %.10f\n", N, sum);

    free(a);
    return 0;
}
