# Dead Code Elimination: The One the Compiler Just Handles

The other write-ups in this directory are about hiding a stall once an
instruction is already in the pipeline. Dead code elimination is different
in kind: it removes instructions whose results are never observable, so
they never get fetched or decoded at all. There's no stall to hide because
there's nothing there.

## Source

[pipeline.dce.cpp](pipeline.dce.cpp) — three patterns, each written twice,
with the dead code left in and taken out by hand:

| Pattern | The dead part |
|---|---|
| **Dead store** | `x = i*i; x = i*i*i; x = i+1;` — the first two are overwritten before anything reads them |
| **Dead branch** | `if (FEATURE_ENABLED)` where `FEATURE_ENABLED` is `constexpr false` |
| **Dead pure call** | `expensivePure(i);` with the return value discarded |

## Results (Apple M-series, N=100M, min of 5 runs)

| Level | Dead store | Dead branch | Dead pure call |
|---|---|---|---|
| `-O0` | 87.9 → 43.5 ms | 47.8 → 46.4 ms | 6238.7 → 47.1 ms |
| `-O1`–`-O3` | 0.00 → 0.00 ms | 0.00 → 0.00 ms | 0.00 → 0.00 ms |

## The compiler handles this one

Removing this dead code by hand is worth something at `-O0` and nothing at
all from `-O1` up. Not "less" — nothing. Each before/after pair compiles to
**byte-identical machine code** once optimization is on, so the version you
cleaned up and the version you didn't are the same function.

It goes further than deleting the dead lines. With those gone, what remains
(`x = i + 1`, `sum += i`) has a closed form, so the compiler replaces the
entire 100-million-iteration loop with a few instructions of arithmetic —
`deadStoreBefore` becomes `bic w0, w0, w0, asr #31`, a branchless
`max(n, 0)`, and that is the whole function. Even the discarded
`expensivePure` call disappears completely despite being marked `noinline`.
That's why every cell above reads `0.00 ms`: past `-O0` there is no loop
left to time.

So the takeaway is not a technique. It's that this particular category of
waste is one you can hand to the optimizer. Write the clear version; don't
hand-inline dead-code removal for performance.
