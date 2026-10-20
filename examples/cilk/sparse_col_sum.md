# Sparse Column Sum: Work-Stealing vs Static and Dynamic Scheduling

## Problem

Given a sparse matrix in CSR (Compressed Sparse Row) format, count the number
of non-zeros in each column:

```
col_count[j] = number of non-zeros in column j
```

CSR stores:
- `row_ptr[i]`: starting index in `col_idx` for row `i`
- `col_idx[nnz]`: column index of each non-zero

The serial algorithm is a simple histogram:

```c
for (int r = 0; r < NROWS; r++)
    for (int p = row_ptr[r]; p < row_ptr[r+1]; p++)
        col_count[col_idx[p]]++;
```

## Why it is hard to parallelize well

The outer loop (rows) is the natural unit of parallelism — each row can be
processed independently. The inner loop writes to `col_count`, which is shared,
but write conflicts can be avoided by giving each worker a private copy of
`col_count` and merging at the end.

The hard part is **load imbalance**: real sparse matrices have wildly varying
row lengths. If some rows have thousands of non-zeros and others have a handful,
a static partition that assigns equal numbers of rows to each thread gives some
threads far more work than others.

## Synthetic test matrix

To make the imbalance concrete and reproducible, the matrix is constructed with:

- `NHEAVY` dense rows at the **start** of the matrix (`HEAVY_NNZ` elements each)
- Remaining rows are sparse (`LIGHT_NNZ` elements each)

The program's built-in defaults are a small 10,000 × 10,000 matrix (100 heavy rows
of 5,000 nnz, the rest 10 nnz). The results and analysis below use a larger
matrix, passed on the command line:

```
./sparse_col_sum 100000 100000 1000 50000 10

matrix: 100000 x 100000
  heavy: first 1000 rows × 50000 nnz each  →  50M nnz
  light: remaining 99000 rows × 10 nnz each →   1M nnz
```

In both, the heavy rows are the first 1% of the matrix. They hold 98% of the
non-zeros in the large matrix and 83% in the default one.

## Results

Platform: Apple M-series, 10 threads/workers, the 100K × 100K matrix above.

| version      | time    | speedup |
|--------------|---------|---------|
| serial       | 23 ms   | 1.0×    |
| omp static   | 26 ms   | 0.9×    |
| omp dynamic  |  6 ms   | 3.9×    |
| cilk         |  5.4 ms | **4.3×** |

## Why each version performs the way it does

### OpenMP static — slower than serial

`schedule(static)` assigns equal-sized contiguous row ranges to each thread.
With 10 threads and 100K rows, thread 0 gets rows 0–9999, which includes
**all 1000 heavy rows**. Thread 0 processes 50M + 90K ≈ 50M non-zeros while
threads 1–9 each process only ~100K non-zeros. The other 9 threads finish
almost instantly and then sit idle waiting for the barrier. The elapsed time is
dominated by thread 0 alone — slower than serial because of the overhead of
spawning threads and the merge step, with zero benefit.

### OpenMP dynamic — 3.9× speedup

`schedule(dynamic, 1)` assigns rows one at a time from a shared work queue.
As soon as a thread finishes a row it pulls the next available one. This
naturally balances the load: threads that finish light rows quickly pick up more
work, and the heavy rows are spread across multiple threads.

The cost is a centralized work queue: one shared counter that every thread
bumps with an atomic fetch-and-add to claim its next row. With 100K rows each
requiring a queue operation, the scheduling overhead is non-trivial — every
claim touches the same cache line, so threads contend on it.

### Cilk — 4.3× speedup

`cilk_for` with `grainsize(1)` makes each row a separate task in a distributed
work-stealing deque. There is no central queue: each worker maintains its own
double-ended deque of tasks and only reaches out to steal from another worker's
deque when its own is empty.

**Grain size is critical.** The default grain size for `cilk_for` lumps
`~N/8P ≈ 1250` rows into each indivisible leaf task. With the heavy rows
concentrated in positions 0–999, the first leaf task `[0, 1250)` captures all
1000 heavy rows and cannot be subdivided once started — collapsing to the same
imbalance as the static schedule. Setting `grainsize(1)` makes each row
stealable and restores full load balance.

With grain 1, work-stealing scales better than dynamic scheduling because:
- **No central contention**: steals happen peer-to-peer between worker deques
  rather than through one shared counter
- **Low steal rate**: once the heavy rows are distributed, most workers rarely
  need to steal — the common case (processing a row already in the local deque)
  has near-zero overhead

## Key takeaway

Work-stealing is not universally faster than dynamic scheduling, but it wins
when:
1. The workload is **severely imbalanced** (not just mildly uneven)
2. There are **many fine-grained tasks** (grain size must match task granularity)
3. **Contention** on a centralized scheduler is the bottleneck

For uniformly sized tasks (like prefix sum), OpenMP static or dynamic with a
good chunk size is simpler and equally fast. Work-stealing pays off when task
sizes are unpredictable and a central queue becomes a bottleneck.

## Building and running

```bash
make
./sparse_col_sum [nrows] [ncols] [nheavy] [heavy_nnz] [light_nnz]

# built-in default (10K × 10K, 100 heavy rows of 5K nnz, rest 10 nnz):
./sparse_col_sum

# the matrix used for the results above (100K × 100K, 1000 heavy rows of 50K nnz):
./sparse_col_sum 100000 100000 1000 50000 10
```
