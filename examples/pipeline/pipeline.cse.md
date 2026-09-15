# Common Subexpression Elimination: Another One the Compiler Handles

Computing the same expression twice re-issues the same instruction and
re-pays its latency. Caching it in a local — common subexpression
elimination — makes the expensive operation run once, and every later use
read a register that's already full.

That's the theory. Mhe practice is that **the compiler does this for you, and doing it by hand is somewhere between
worthless and actively harmful.**

## Source

[pipeline.cse.cpp](pipeline.cse.cpp) — three redundant expressions, each
written twice, with the subexpression recomputed and hoisted by hand:

| Section | The repeated expression |
|---|---|
| **1 · integer** | `a*b + c`, used three times |
| **2 · loop index** | `arr[stride*i]`, indexed three times per iteration |
| **3 · float** | `sqrt(x*x + y*y + z*z)`, called three times |

## Results (Apple M-series, min of 5 runs, before → after)

| Level | 1 · integer | 2 · loop index | 3 · float |
|---|---|---|---|
| `-O0` | 155.6 → 183.7 ms (**0.85x**) | 242.9 → 310.1 ms (**0.78x**) | 43.3 → 51.4 ms (**0.84x**) |
| `-O1` | 0.00 → 0.00 ms | 73.4 → 73.4 ms (1.00x) | 11.6 → 9.5 ms (1.23x) |
| `-O2` | 0.00 → 0.00 ms | 12.7 → 11.3 ms (1.13x) | 6.70 → 6.33 ms (1.06x) |
| `-O3` | 0.00 → 0.00 ms | 12.8 → 11.3 ms (1.13x) | 6.70 → 6.31 ms (1.06x) |

## The compiler does the CSE

From `-O1` up, every genuine common subexpression here is eliminated
without help. Section 1 doesn't just get CSE'd, it evaporates — both
variants read `0.00 ms` because once the redundant multiplies are gone the
remaining loop has a closed form and no loop survives to time. Section 2
ties exactly at `-O1`.

**At `-O0`, hand-CSE is consistently *slower* — all three sections, 0.78x
to 0.85x.** That's not noise. The hoisted versions introduce extra named
locals (`base`, `idx`, `elem`, `lenSq`, `invLen`), and with no register
allocation each one gets its own stack slot with store/load traffic on
every iteration. Recomputing `a*b + c` inline is cheaper than spilling it
and loading it back.
