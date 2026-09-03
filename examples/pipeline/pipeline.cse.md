# Common Subexpression Elimination: Paying a Stall Once, Not Three Times

Recomputing the same expression re-issues the same multi-cycle instruction
and re-pays its stall every time. Caching the result in a local variable —
common subexpression elimination (CSE) — means the expensive operation
executes once, and every reuse reads an already-available register.

## Source

[pipeline.cse.cpp](pipeline.cse.cpp)

## How it works

**Before — the same multiply repeated three times:**

```cpp
int beforeCse(int a, int b, int c) {
    int x = a * b + c;          // chain 1: MUL → stall → ADD
    int y = a * b + c + 1;      // chain 2: MUL again → stall → ADD → ADD
    int z = a * b + c + 2;      // chain 3: MUL again → stall → ADD → ADD
    return x + y + z;
}
```

```
Cycle  Instruction
-----  -----------
 1     MUL  t0, a, b        ; a*b  (result ready cycle 4)
 2     --- stall ---
 3     --- stall ---
 4     ADD  x,  t0, c       ; x = a*b + c
 5     MUL  t1, a, b        ; DUPLICATE (result ready cycle 8)
 6     --- stall ---
 7     --- stall ---
 8     ADD  y,  t1, c       ; y = a*b + c  (same value!)
                              ↑ 6 wasted stall cycles, and a third repeat below
```

**After — computed once, reused as a ready register:**

```cpp
int afterCse(int a, int b, int c) {
    const int base = a * b + c; // ONE multiply, ONE dependency chain
    const int x = base;         // register rename / copy — free
    const int y = base + 1;     // ADD on already-available register — free
    const int z = base + 2;     // ADD on already-available register — free
    return x + y + z;
}
```

```
Cycle  Instruction
-----  -----------
 1     MUL  t0, a, b        ; a*b once (result ready cycle 4)
 2     --- stall ---
 3     --- stall ---
 4     ADD  base, t0, c     ; base = a*b + c
 5     MOV  x,   base       ; x = base     ← no stall: base ready
 6     ADD  y,   base, 1    ; y = base + 1  ← no stall
 7     ADD  z,   base, 2    ; z = base + 2  ← no stall
                              ↑ 0 wasted stall cycles
```

The source benchmarks this same shape three times: the integer multiply
above, an array index (`arr[stride * i]`, recomputed on every access inside
a loop), and a floating-point `sqrt()` call (`sqrt(x² + y² + z²)`, the
highest-latency operation of the three). The `before*`/`after*` functions
are ordinary code — no `NOINLINE`, no optimizer fences — so the compiler is
free to apply the same CSE to the "before" variants itself, and at `-O1`
and up, for two of the three sections, it does.

## Build

```bash
clang++ -O2 -o pipeline.cse pipeline.cse.cpp -lm && ./pipeline.cse
```

## Results (Apple M-series, min of 12 runs per cell)

| Level | Integer (`a*b+c`) | Array index (`stride*i`) | `sqrt` |
|---|---|---|---|
| `-O0` | 158 / 187 ms — **0.85x** (before is faster) | 245 / 307 ms — **0.80x** (before is faster) | 42 / 51 ms — **0.83x** (before is faster) |
| `-O1` | 22.5 / 22.5 ms — converged | 74 / 72 ms — converged | 11.6 / 9.4 ms — **1.23x** |
| `-O2` | 22.6 / 22.5 ms — converged | 11.3 / 11.3 ms — converged | 6.7 / 6.3 ms — **1.06x** |
| `-O3` | 22.5 / 22.5 ms — converged | 11.3 / 11.3 ms — converged | 6.7 / 6.3 ms — **1.06x** |

"Converged" means the before/after floors land on the same number to within
run-to-run noise — there's no reliable gap left to report once the compiler
is allowed to inline and optimize the "before" variant itself.

## Analysis

**The integer and array-index cases are already solved by `-O1`.** Without
`NOINLINE` forcing a real function call, the compiler inlines `beforeCse`
and `arraySumBefore` into their benchmark loops and its own CSE pass finds
the exact redundancy the "after" rewrite targets by hand. The two variants
compile down to equivalent code, so their timings converge — the same thing
[pipeline.md](pipeline.md)'s dependent-chain example shows for a different
optimization: the effect is real, but only visible at levels where the
compiler hasn't already automated the fix.

**`sqrt()` is the exception**, and it's real: dumping the `-O3` assembly
shows `normalizeBefore` still contains three separate `fsqrt` instructions
where `normalizeAfter` has one. The compiler can't merge them because a libm
`sqrt()` call is allowed to set `errno` on a domain error (unless built with
`-fno-math-errno` or `-ffast-math`) — an observable side effect the as-if
rule won't let the compiler optimize past. Three calls with identical
arguments aren't provably redundant, so this is one of the few cases here
where the by-hand rewrite still buys something even when the compiler is
otherwise left free to do its job.

**`-O0` shows a real, backwards effect on all three sections**: "before" is
consistently *faster* than "after". At `-O0` nothing is register-allocated
across statements, so the "after" variants' extra named locals (`base`,
`idx`/`elem`, `lenSq`/`invLen`) round-trip through the stack more than the
"before" versions' inline recomputation does. Same phenomenon documented in
[loop_unrolling.md](../loop_optimizations/loop_unrolling.md)'s `-O0` result
— a debugging-only optimization level can penalize a rewrite that only pays
off once real optimization is turned on.

## Key takeaways

1. **Most hand-CSE here turns out to be something `-O1` already does for
   you.** Once nothing (like a forced `NOINLINE`) stops the compiler from
   seeing across the call, its ordinary CSE pass finds the same redundancy
   the manual rewrite targets.
2. **The case that survives is the one the compiler isn't allowed to
   touch.** `sqrt()`'s `errno` contract blocks CSE across identical calls
   unless you relax it — this is a case where doing the rewrite by hand (or
   building with `-fno-math-errno`) genuinely helps at every level.
3. **`-O0` is not a performance baseline.** It can penalize a "cleaner"
   refactor for reasons that have nothing to do with the optimization being
   taught — see the as-if rule and optimization-level discussion in
   [01.compiler_optimization.md](../../course_materials/01.compiler_optimization.md).
4. **Trust the floor, not a single run, when the effect is this small.**
   The median timings here made `-O1`..`-O3`'s integer case look like a real
   ~1.3–1.4x win; the minimum over many runs shows that's noise, not signal.
