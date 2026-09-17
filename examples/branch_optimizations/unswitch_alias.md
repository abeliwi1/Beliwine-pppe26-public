# Loop Unswitching Blocked by Aliasing

## Source

[unswitch_alias.cpp](unswitch_alias.cpp)

## The transformation

A loop tests the same condition on every iteration, and the condition never
changes:

```cpp
for (int i = 0; i < n; i++) {
    if      (*mode == 0.0f) c[i] = a[i] + b[i];
    else if (*mode == 1.0f) c[i] = a[i] * b[i];
    else                    c[i] = fabsf(a[i]) + fabsf(b[i]);
}
```

**Loop unswitching** hoists that test out and specialises the loop per outcome:

```cpp
float m = *mode;
if      (m == 0.0f) for (int i = 0; i < n; i++) c[i] = a[i] + b[i];
else if (m == 1.0f) for (int i = 0; i < n; i++) c[i] = a[i] * b[i];
else                for (int i = 0; i < n; i++) c[i] = fabsf(a[i]) + fabsf(b[i]);
```

Removing the test is the small win. The real win is what it enables: each
specialised body is now straight-line elementwise arithmetic, which the
vectoriser will take. A loop with a branch in the middle of it, it will not.

## Why the condition is read through a pointer

This is the whole design of the example, so it is worth being explicit. Two
properties of `const float* mode` are load-bearing, and removing either one lets
the compiler do the transformation itself.

**It is a pointer, not a value.** A by-value parameter cannot change during the
loop — nothing in the body can reach it — so invariance is immediate and the
compiler hoists the test unprompted. An indirect read has to be re-justified on
every iteration: the compiler must prove no store in the body can reach the
pointed-to object.

**It points to the same type as the output.** Both are `float`. C++ strict
aliasing permits the compiler to assume objects of unrelated types never
overlap, so `const int* mode` would be hoisted freely — an `int` and a `float`
cannot alias, and the proof succeeds trivially. Matching the types closes that
escape. `c[i] = ...` genuinely might write through `mode`, because a caller is
entitled to pass `mode = c`.

The consequence: `*mode` is reloaded and re-tested every iteration. The
condition cannot leave the loop, so the loop cannot be specialised, so it
cannot be vectorised. It stays one scalar loop.

## What the compiler generates

| level | `apply_switched` | `apply_unswitched` |
|-------|------------------|--------------------|
| `-O2` | 1 loop, 0 vector ops | 6 loops, 20 vector ops |
| `-O3` | **1 loop, 0 vector ops** | 6 loops, 20 vector ops |

## Results (Apple M5, N=32M floats, Apple Clang 17)

| mode | level | switched | unswitched | speedup |
|------|-------|----------|------------|---------|
| 0 (add) | `-O1` | 9.2 ms | 7.7 ms | 1.20x |
| 1 (mul) | `-O1` | 10.2 ms | 7.7 ms | 1.32x |
| 2 (abssum) | `-O1` | 15.3 ms | 7.8 ms | 1.98x |
| 0 (add) | `-O2` | 9.4 ms | 3.6 ms | **2.60x** |
| 1 (mul) | `-O2` | 10.2 ms | 3.4 ms | **2.99x** |
| 2 (abssum) | `-O2` | 15.3 ms | 3.4 ms | **4.52x** |
| 0 (add) | `-O3` | 8.3 ms | 3.4 ms | **2.43x** |
| 1 (mul) | `-O3` | 15.2 ms | 3.5 ms | **4.40x** |
| 2 (abssum) | `-O3` | 15.2 ms | 3.4 ms | **4.42x** |

The thing to read off this table is that the gap **does not close at `-O3`** —
the manual transformation is still worth 2.4x to 4.4x at the highest level the
compiler offers.

The three unswitched timings are also nearly identical (3.4–3.5 ms) regardless
of mode, because all three specialised loops vectorise and then hit the same
memory-bandwidth floor. The switched version's times vary by mode, because it is
executing genuinely different scalar work each time.

## Legality, not budget

The distinction matters for how you read any "the compiler didn't optimise
this."

The compiler cannot hoist `*mode` out of the loop because doing so **might
change behaviour** for a caller it cannot rule out. If someone really did pass
`mode = c`, the load would have to be repeated, and hoisting it would be
observably wrong. That is a legality refusal, and no optimisation level or flag
lifts it — it is the reason the switched version stays one scalar loop even at
`-O3`.

(Budget refusals also exist and look superficially similar. Once the hoist is
done by hand, whether the compiler goes on to *split* the loop is a cost
question, and `-O2` and `-O3` answer it differently — see
[Is the hoist alone enough?](#is-the-hoist-alone-enough) below. The two
refusals need different responses, which is why it is worth telling them
apart.)

That is the same shape as a floating-point reduction that will not vectorise
without `-ffast-math`: the optimiser has the capability and lacks the
permission. The difference is that `-ffast-math` grants permission globally and
changes results, whereas here you grant it locally, by performing the hoist
yourself, and the results are unchanged.

## A boundary condition: these kernels are `noinline`, and must be

If the compiler can see the call site, it can see what `mode` actually points
at, prove it does not alias `c` for that specific call, and the effect
disappears. Measured with inlining permitted: **1.06x** instead of 2.43x.

That is not a flaw in the example, it is the example's scope. The aliasing
problem is a property of a function compiled without knowledge of its caller —
a routine in another translation unit, taking a settings or context pointer, too
large to inline. `noinline` models that; without it, the file measures inlining
rather than unswitching.

It is also a reminder about benchmark construction generally: this effect was
visible in the generated assembly (1 scalar loop) while the first timing run
said 1.06x. When the code and the clock disagree, one of them is measuring
something you did not intend.

## Is the hoist alone enough?

The transformation done by hand has two parts, and they are worth separating
because the compiler treats them differently:

```cpp
float m = *mode;                        // part 1: the hoist
if      (m == 0.0f) for (...) ...       // part 2: the split
else if (m == 1.0f) for (...) ...
else                for (...) ...
```

A reasonable question is whether part 1 is sufficient on its own — once `m` is a
local that nothing can write to, invariance is provable, so why not let the
compiler take it from there? Measured three ways: no change, hoist only (branch
left inside the loop), and hoist plus manual split.

| | `-O1` | `-O2` | `-O3` |
|---|---|---|---|
| no change | 1 loop, 0 vector | 1 loop, 0 vector | 1 loop, 0 vector |
| **hoist only** | 1 loop, 0 vector | **1 loop, 0 vector** | **6 loops, 20 vector** |
| hoist + split | 3 loops, 0 vector | 6 loops, 20 vector | 6 loops, 20 vector |

| mode | `-O2` hoist only | `-O2` hoist + split | `-O3` hoist only | `-O3` hoist + split |
|------|------------------|---------------------|------------------|---------------------|
| 0 (add) | 1.10x | **2.71x** | 2.41x | 2.41x |
| 1 (mul) | 1.17x | **3.34x** | 4.44x | 4.44x |
| 2 (abssum) | 1.27x | **5.67x** | 4.58x | 4.52x |

**At `-O3`, the hoist alone is enough.** With invariance provable the compiler
unswitches unprompted and vectorises the result; hoist-only and hoist-plus-split
are indistinguishable at 3.4 ms.

**At `-O2` it is not.** The hoist buys 1.10x–1.27x where the full transformation
buys 2.71x–5.67x — less than a quarter of the available win.

### Why the two parts differ

They are blocked by different things.

* **The hoist** is a *legality* question: can the compiler prove the store to
  `c[i]` leaves `*mode` intact? Aliasing says no, at every optimisation level,
  and no flag changes that. Only you can resolve it.
* **The split** is a *budget* question: is duplicating the loop body three times
  worth it? That is a cost model, and `-O3` raises the unswitching threshold
  enough to say yes where `-O2` says no.

You can see the budget half in isolation by passing the selector **by value**
instead of by pointer. That is provably invariant with no hoist needed — and
`-O2` still declines to unswitch it (1 loop, 0 vector ops), while `-O3` does.
So `-O2`'s reluctance has nothing to do with the aliasing; it is simply a
narrower cost threshold.

### What to write

Write both lines. They cost nothing, they are correct at every level, and the
diagnosis differs if you only write one: if hoisting alone does not help, the
compiler has stopped refusing on legality and started refusing on budget — and
the fix for a budget refusal is to perform the duplication yourself.

---

## Key takeaways

1. **Invariance the compiler cannot prove is invariance you must exploit
   yourself.** The condition here never changes, and the compiler knows it never
   changes in practice — it just cannot rule out the caller who makes it change.

2. **Aliasing is decided by type, not by intent.** Passing the selector as
   `const int*` instead of `const float*` would have let the compiler hoist it,
   because strict aliasing says unrelated types do not overlap. The bug-shaped
   version and the fast version differ by a type declaration.

3. **Hoisting a loop-invariant load unblocks a whole pipeline of
   optimisations.** `float m = *mode;` does not just remove a comparison — it
   makes the loop specialisable, which makes it vectorisable, which is where the
   3–5x actually comes from. Whether you also have to perform the split by hand
   depends on the optimisation level: at `-O3` the compiler takes it from there,
   at `-O2` it does not. Write both parts.
