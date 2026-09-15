# Source Order: Does It Matter Where You Write the Dependency?

[pipeline.md](pipeline.md) breaks one long dependency chain into four
independent ones and measures the speedup. This variant asks a narrower
follow-up question: given four independent chains, does it matter *how you
order the statements* that update them?

Two ways to write the identical computation:

**Grouped by chain** (what [pipeline.cpp](pipeline.cpp) does) — each chain's
four multiplies sit together, so every line reads what the line directly
above it just wrote:

```cpp
x1 = x1 * a;  x1 = x1 * (a + 2);  x1 = x1 * (a + 4);  x1 = x1 * (a + 6);
x2 = x2 * b;  x2 = x2 * (b + 2);  x2 = x2 * (b + 4);  x2 = x2 * (b + 6);
x3 = ...
x4 = ...
```

**Interleaved by operation** (what [pipeline.nochains.cpp](pipeline.nochains.cpp)
does) — one multiply across all four chains, then the next, so no line
depends on the line above it. Each statement's operand was written four
lines earlier:

```cpp
x1 = x1 * a;        x2 = x2 * b;        x3 = x3 * c;        x4 = x4 * d;
x1 = x1 * (a + 2);  x2 = x2 * (b + 2);  x3 = x3 * (c + 2);  x4 = x4 * (d + 2);
x1 = x1 * (a + 4);  x2 = x2 * (b + 4);  x3 = x3 * (c + 4);  x4 = x4 * (d + 4);
x1 = x1 * (a + 6);  x2 = x2 * (b + 6);  x3 = x3 * (c + 6);  x4 = x4 * (d + 6);
```

Same 16 statements, same four accumulators, same dependency *graph*, same
total work, same answer. Only the source order differs. The two files are
otherwise byte-identical, so this is a clean A/B.

("nochains" is a slightly loose name: the four chains still exist and are
just as long. What's removed is *successive-line* dependency.)

## Source

[pipeline.nochains.cpp](pipeline.nochains.cpp) — compare against
[pipeline.cpp](pipeline.cpp)

## Build

```bash
clang++ -std=c++17 -O0 -o pipeline.nochains pipeline.nochains.cpp && ./pipeline.nochains
clang++ -std=c++17 -O1 -o pipeline.nochains pipeline.nochains.cpp && ./pipeline.nochains
```

## Results (Apple M-series, N=100M ints, min of 5 runs)

Only `independentChains` differs between the files. `dependentChain`
measures identically in both (326 ms at `-O0`, 269 ms at `-O1`), which
confirms the comparison is isolating the ordering change and nothing else.
Both files print `Same answer? YES` at every level.

| Level | Grouped by chain | Interleaved by operation | |
|---|---|---|---|
| `-O0` | 179 ms | **168 ms** | **1.065x faster interleaved** |
| `-O1` | 69 ms | 69 ms | exact tie |
| `-O2` | 69 ms | 69 ms | tie |
| `-O3` | 69 ms | 70 ms | tie |

**Source order buys you something at `-O0` and nothing at all once
optimization is on.** The `-O0` win is small but stable and reproducible
(runs cluster tightly at 179 and 168); from `-O1` up the two orderings are
indistinguishable.

## Why it wins at `-O0`

At `-O0` nothing lives in a register across statements — every accumulator
round-trips through a stack slot after every operation. So the dependencies
that matter here aren't register dependencies, they're **memory**
dependencies: a load that has to wait for the store to the same address
just above it.

Grouped by chain, all four of `x1`'s multiplies hit the *same* stack slot
back to back:

```
ldur x8, [x29,#-16]   ; load x1
stur x8, [x29,#-16]   ; store x1
ldur x8, [x29,#-16]   ; load x1 again, immediately
stur x8, [x29,#-16]
ldur x8, [x29,#-16]   ; ...four store->load round trips, one address
stur x8, [x29,#-16]
```

Interleaved, the same work rotates across four different slots:

```
ldur x8, [x29,#-16]   ; x1
stur x8, [x29,#-16]
ldur x8, [x29,#-24]   ; x2
stur x8, [x29,#-24]
ldur x8, [x29,#-32]   ; x3
stur x8, [x29,#-32]
```

Each slot's store-to-load-forwarding latency now has two other chains'
worth of independent work sitting between it and the next access to that
same address, so it overlaps instead of stalling.

## Why it stops helping at `-O1`

Once optimization is on, the accumulators live in registers, the stack
round trips disappear, and — critically — **LLVM's instruction scheduler
reorders independent operations itself, regardless of how you wrote them**.

Both files compile `independentChains` to **exactly 55 instructions** at
`-O1`. The two sequences aren't identical, but they're the same
instructions in a slightly different order — the compiler has already
interleaved the grouped-by-chain source on its own. On top of that, the
CPU's out-of-order engine issues by operand readiness at runtime, not fetch
order. Two schedulers now sit between your source order and what actually
executes, and they erase the difference completely: 69 ms either way.

At `-O2` and `-O3` the question stops being meaningful at all, because the
compiler also splits `dependentChain` into four lanes by itself (see
[pipeline.md](pipeline.md)) — every version converges on the same 69 ms.

## Key takeaways

1. **Source order matters exactly when the compiler isn't reordering for
   you.** At `-O0` — no scheduling, everything through memory — writing the
   statements so consecutive lines touch different accumulators is worth
   about 6%. At `-O1` and above it is worth nothing, because the compiler
   already does it.
2. **At `-O0` the independence you're buying is store-to-load forwarding,
   not ALU parallelism.** Nothing is register-resident; the win is that
   consecutive memory dependencies land on different addresses.
3. **How much that's worth depends on what you're overlapping.** The same
   reordering bought 1.29x against single-cycle bit-mixing ops and 1.065x
   against multiplies, because the stall being hidden is a smaller fraction
   of a multiply's cost. The mechanism didn't change; the ratio did.
4. **Two schedulers stand between your source and the hardware**: LLVM's
   static instruction scheduler and the CPU's out-of-order engine. Both
   work from the dependency graph, not from the order you typed. Hand-tuning
   statement order is redundant with work they already do.
