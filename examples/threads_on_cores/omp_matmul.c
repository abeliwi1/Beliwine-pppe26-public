// omp_matmul.c -- one process, K threads, repeating a matrix multiply.
//   OMP_NUM_THREADS=K ./omp_matmul SECONDS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <omp.h>

#define N 1024

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    double seconds = argc > 1 ? atof(argv[1]) : 10.0;
    prctl(PR_SET_NAME, "matmul");
    float* A = malloc(sizeof(float) * N * N);
    float* B = malloc(sizeof(float) * N * N);
    float* C = malloc(sizeof(float) * N * N);
    for (int i = 0; i < N * N; i++) { A[i] = (i % 7) * 0.5f; B[i] = (i % 5) * 0.25f; }

    printf("%d\n", getpid());
    fflush(stdout);

    double end = now() + seconds;
    int reps = 0, done = 0;
    // One parallel region for the whole run, so the threads never go idle
    // between multiplies: each repetition is an omp for over the rows of C.
    #pragma omp parallel
    {
        while (!done) {
            #pragma omp for
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) C[i * N + j] = 0.0f;
                for (int k = 0; k < N; k++) {
                    float a = A[i * N + k];
                    for (int j = 0; j < N; j++) C[i * N + j] += a * B[k * N + j];
                }
            }                                        // implicit barrier
            #pragma omp single
            { reps++; done = now() >= end; }         // implicit barrier
        }
    }
    fprintf(stderr, "matmul: %d threads, %d multiplies, C[0] = %g\n",
            omp_get_max_threads(), reps, C[0]);
    return 0;
}
