// fork_workers.c -- N single-threaded processes, each burning one core.
//   ./fork_workers N SECONDS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// Monte Carlo estimate of pi: CPU-bound, no locks, no memory traffic.
static void work(double seconds, uint64_t seed) {
    uint64_t x = seed | 1, inside = 0, total = 0;
    double end = now() + seconds;
    while (now() < end) {
        for (int i = 0; i < 1000000; i++) {
            x ^= x << 13; x ^= x >> 7; x ^= x << 17;       // xorshift64
            double a = (x & 0xffffffff) / 4294967296.0;
            double b = (x >> 32) / 4294967296.0;
            inside += a * a + b * b < 1.0;
        }
        total += 1000000;
    }
    printf("worker %d: pi ~ %.6f\n", getpid(), 4.0 * inside / total);
}

int main(int argc, char** argv) {
    int n = argc > 1 ? atoi(argv[1]) : 4;
    double seconds = argc > 2 ? atof(argv[2]) : 10.0;

    for (int i = 0; i < n; i++) {
        fflush(stdout);                               // or the child inherits our buffer
        pid_t pid = fork();
        if (pid == 0) {                               // child
            prctl(PR_SET_NAME, "worker");
            work(seconds, 0x9e3779b97f4a7c15ULL * (i + 1));
            return 0;
        }
        printf("%d\n", pid);                          // parent: report child's PID
    }
    fflush(stdout);
    while (wait(NULL) > 0) ;                          // parent waits for every child to finish
    return 0;
}
