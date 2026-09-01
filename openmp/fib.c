#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

/*
 *  fib.c  --  #pragma omp task and #pragma omp taskwait
 *
 *  The two fundamental OpenMP task primitives:
 *
 *    #pragma omp task    Create a task from the following statement. Any
 *                         idle thread in the team MAY run it -- the runtime
 *                         decides when and where, same as cilk_spawn.
 *
 *    #pragma omp taskwait Wait here until every task created by THIS task
 *                         (or the enclosing region) has completed.
 *
 *  Unlike cilk_spawn, `task` only creates work -- it does not create a
 *  thread team. Tasks must be generated from inside a `parallel` region,
 *  and to avoid every thread in the team redundantly running the whole
 *  recursion, exactly one thread should generate the task tree: wrap the
 *  top-level call in `#pragma omp single`.
 *
 *  A variable written inside a task and read after `taskwait` must be
 *  marked `shared` -- the OpenMP default for task-local variables is
 *  firstprivate (a private copy), so without the clause the write would
 *  be lost.
 *
 *  Fibonacci is the textbook example: not efficient (exponential work), but
 *  the two recursive calls are independent, so it shows the primitives clearly.
 *
 *  Compile (clang + libomp on macOS):
 *    clang -Xpreprocessor -fopenmp -O3 \
 *      -I/opt/homebrew/opt/libomp/include \
 *      -L/opt/homebrew/opt/libomp/lib -lomp \
 *      fib.c -o fib
 *
 *  Usage: ./fib [n]        (default n = 40)
 */

long fib(int n)
{
    if (n < 2)
        return n;

    long a, b;

    #pragma omp task shared(a)
    a = fib(n - 1);         /* may run in parallel ...            */
    b = fib(n - 2);         /* ... with this continuation         */

    #pragma omp taskwait     /* both must finish before we add     */

    return a + b;
}

int main(int argc, char *argv[])
{
    int n = argc > 1 ? atoi(argv[1]) : 40;
    long result;

    #pragma omp parallel
    {
        #pragma omp single
        result = fib(n);
    }

    printf("fib(%d) = %ld\n", n, result);
    return 0;
}
