# Shared data in OpenMP (draft)

Parked from the first draft of Lecture 10, which was restructured to put
parallelism first: how work is divided, scheduling, and load balance. This
material belongs to a later lecture on shared data, alongside races, locks,
atomics and Cilk reducers. It refers to Lecture 09's race (`sum += a[i]`) and
its rule: "private by default, share explicitly, synchronize every shared
write."

Examples that go with it: [reduction.c](../../examples/openmp/reduction.c),
[dot_product.c](../../examples/openmp/dot_product.c), and the `reduction`
cells of [openmp_intro.ipynb](../../examples/openmp/openmp_intro.ipynb).

---

## Shared and private

Inside a parallel region, every variable is either **shared**, meaning one copy
seen by every thread, or **private**, meaning each thread has its own copy. In
Lecture 09's terms, a private variable lives on each thread's own stack.
Getting this right is most of the work of writing correct OpenMP.

### The default rules

| variable | default |
|---|---|
| declared **outside** the region, including globals and `static` variables | **shared** |
| declared **inside** the region | private |
| the index of a loop under `omp for` | private |

The first row is the one that matters. **OpenMP's default is shared**, the
opposite of Lecture 09's rule. Any variable declared before the pragma and used
inside the region is shared, whether or not you meant it to be.

Here is the bug that follows:

```c
double tmp;                       // declared outside: shared

#pragma omp parallel for
for (long i = 0; i < N; i++) {
    tmp = a[i] * a[i];            // every thread writes the same tmp
    b[i] = tmp + 1.0;             // ... and may read another thread's value
}
```

Between one thread's write to `tmp` and its read, another thread can overwrite
it. The program runs, usually gives the right answer, and is sometimes wrong.
The fix is to make `tmp` private, most simply by declaring it where it is used:

```c
#pragma omp parallel for
for (long i = 0; i < N; i++) {
    double tmp = a[i] * a[i];     // declared inside: one per thread
    b[i] = tmp + 1.0;
}
```

### Clauses that change the default

When a variable has to be declared outside, a **data-sharing clause** on the
pragma says what each thread gets:

| clause | each thread gets |
|---|---|
| `shared(x)` | no copy; all threads use the original `x` |
| `private(x)` | its own `x`, **uninitialized**; the original isn't updated afterward |
| `firstprivate(x)` | its own `x`, initialized to the original's value |
| `lastprivate(x)` | its own `x`; after the loop, the original gets the value from the last iteration |

```c
double scale = 2.0, tmp;

#pragma omp parallel for private(tmp) firstprivate(scale)
for (long i = 0; i < N; i++) {
    tmp = a[i] * scale;
    b[i] = tmp + 1.0;
}
```

`private` is the one that surprises people: the private copy starts with
garbage, not with the value the variable had before the region.

### Pointers: the copy is of the pointer

Arrays are usually reached through a pointer, and a private pointer is a copy
of the *pointer*, not of the memory it points to:

```c
double *a = malloc(N * sizeof(double));

#pragma omp parallel firstprivate(a)
{
    a[0] = 1.0;     // every thread's copy of a points at the same array:
}                   // this is a shared write, and a race
```

The heap is shared by every thread, as Lecture 09 said. Making a pointer private
doesn't change that.

### `default(none)`: make every decision visible

The clause `default(none)` turns off the default rules. Every variable declared
outside the region must then appear in a data-sharing clause, or the compiler
rejects the program:

```c
#pragma omp parallel for default(none) shared(a, b, N)
for (long i = 0; i < N; i++) {
    double tmp = a[i] * a[i];
    b[i] = tmp + 1.0;
}
```

This is Lecture 09's rule enforced by the compiler: nothing is shared by
accident. It costs a longer pragma, and it is worth using whenever a region
touches more than a couple of variables.

---

## `reduction`: combining results without a race

Here is Lecture 09's race in OpenMP:

```c
double sum = 0.0;                 // shared

#pragma omp parallel for
for (long i = 0; i < N; i++)
    sum += a[i];                  // load, add, store: updates get lost
```

`sum` has to be shared, because the answer has to end up in one place. But
every thread updating it is exactly the lost-update race. Applying the rule
"private by default, share explicitly, synchronize every shared write" gives
this:

```c
double sum = 0.0;

#pragma omp parallel
{
    double my_sum = 0.0;          // private: a partial sum on this thread's stack
    #pragma omp for
    for (long i = 0; i < N; i++)
        my_sum += a[i];           // no sharing in the loop at all

    #pragma omp atomic
    sum += my_sum;                // one synchronized shared write per thread
}
```

Each thread accumulates privately, and there are only T shared writes, one per
thread. `atomic` makes each of those writes indivisible, using the hardware's
locked instructions from Lecture 09. This pattern is so common that OpenMP has a clause for it:

```c
double sum = 0.0;

#pragma omp parallel for reduction(+:sum)
for (long i = 0; i < N; i++)
    sum += a[i];
```

`reduction(+:sum)` does what the hand-written version does:

  1. Each thread gets a private copy of `sum`, initialized to the **identity**
     of the operator (0 for `+`, 1 for `*`).
  2. Each thread accumulates into its own copy.
  3. At the end of the loop, the private copies are combined with the operator
     and added into the original `sum`.

The built-in operators are `+`, `*`, `&`, `|`, `^`, `&&`, `||`, `min` and `max`.
For other types or other combining functions, `#pragma omp declare reduction`
defines your own by giving the combining step and the initial value.

Two consequences:

  * **The result can differ from the serial result in the last few digits.**
    The partial sums are added in a different order, and floating-point
    addition isn't associative. Both answers are correct to within rounding.
  * **It is fast.** The loop itself has no shared writes, so the threads never
    fight over the cache line that holds `sum`.
    [dot_product.c](../../examples/openmp/dot_product.c) times a reduction against
    the serial loop and reports the memory bandwidth it reaches. Run it in the
    lab.
