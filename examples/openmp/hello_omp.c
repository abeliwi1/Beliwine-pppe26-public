#include <stdio.h>
#include <omp.h>

/*
 *  hello_omp.c  --  #pragma omp parallel
 *
 *  The fundamental OpenMP primitive: forks a TEAM of threads that all run
 *  the following block. There's no direct Cilk equivalent -- cilk_spawn
 *  creates work on an existing worker pool, while `parallel` is what
 *  creates the pool (the team) in the first place. Every other OpenMP
 *  construct (parallel for, task, sections, ...) must run inside one.
 *
 *  Number of threads defaults to the core count; override with
 *  OMP_NUM_THREADS=<n> ./hello_omp or omp_set_num_threads(n).
 *
 *  Compile (clang + libomp on macOS):
 *    clang -Xpreprocessor -fopenmp -O3 \
 *      -I/opt/homebrew/opt/libomp/include \
 *      -L/opt/homebrew/opt/libomp/lib -lomp \
 *      hello_omp.c -o hello_omp
 */

int main() {
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();
        printf("Hello from thread %d of %d\n", tid, nthreads);
    }
    return 0;
}
