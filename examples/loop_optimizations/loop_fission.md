# Loop Fission

Splitting a high-register-pressure loop into several lower-pressure loops to
eliminate compiler spills to the stack. This is the one example here where the
textbook mechanism is exactly what happens, and where the assembly says so out
loud: **the speedup tracks the spill count line for line.**

## Source

[loop_fission.cpp](loop_fission.cpp)

## Machine

AMD Ryzen AI 9 HX 370 (Zen 5, "Strix Point"), Linux, g++ 13.3. The number that
matters for this example is the register file: x86-64 has **16 architectural
XMM registers** and 16 general-purpose registers.

## What is loop fission?

Loop fission (loop distribution) is the inverse of [loop fusion](loop_fusion.md):
one loop with a wide body is split into several loops over the same iteration
range. The motivation is register pressure — when a loop body needs more
simultaneously-live values than the machine has registers, the compiler spills
the excess to the stack and pays a load and a store per spilled value per
iteration. Splitting reduces the live set per loop and can remove the spills
entirely.

The cost is that anything read by every slice gets re-read once per slice. So
there is an optimum, and it is not "as small as possible."

## Why this example

Multiple simultaneous dot products (cross-correlations) of one signal against
many reference vectors. Candidates that don't work:

- **Statistics (sum, mean, variance, max, min)** — only 4–6 accumulators,
  nowhere near enough to exhaust a register file.
- **Polynomial moments (sum, sum², sum³, sum⁴)** — same problem, and the
  higher powers add compute rather than pressure.
- **Multiple correlations** — each needs its own independent FP accumulator.
  With 32 correlations you need 32 accumulators plus 32 reference pointers plus
  a signal pointer and a loop counter, against 16 XMM and 16 GP registers.

The structure is also practically motivated: matched filters, pattern
matching, and audio cross-correlation all run one signal against many
references at once. It is a realistic case where a programmer writes one wide
loop and is surprised.

## How it works

**Unfissioned — one loop, 32 accumulators:**

```cpp
float a0 = 0.0f, a1 = 0.0f, /* ... */ a31 = 0.0f;
for (int i = 0; i < n; i++) {
    float s = signal[i];
    a0  += s * ref[ 0][i];
    a1  += s * ref[ 1][i];
    // ...
    a31 += s * ref[31][i];
}
c[0] = a0;  /* ... */  c[31] = a31;
```

**Fissioned — four loops of 8 accumulators each:**

```cpp
// correlations 0-7
float a0 = 0.0f, /* ... */ a7 = 0.0f;
for (int i = 0; i < n; i++) {
    float s = signal[i];
    a0 += s * ref[0][i];  // ...  a7 += s * ref[7][i];
}
c[0] = a0;  /* ... */  c[7] = a7;

// correlations 8-15, 16-23, 24-31 likewise
```

Note the accumulators are **local variables**, not slots in the output array.
This matters: if you accumulate directly into `c[k]`, the compiler cannot prove
that writing `c[k]` does not alias `ref[j][i]` or the `ref` pointer array, so
it reloads the pointer and the accumulator from memory on every iteration and
never puts them in registers at all. You get a slow loop with *zero* spills —
a different problem with a different fix, and not a demonstration of register
pressure.

[loop_fission_spill.html](loop_fission_spill.html) draws what the two versions
above do to the register file: the two banks as slots, filling as the loop body
widens, and the values with nowhere left to go dropping into stack slots. Nothing
spills at 8 accumulators — which is exactly where the measured knee turns out to
be. Standalone; double-click it.

## Build

```bash
g++ -O1 -o fission_O1 loop_fission.cpp && ./fission_O1
```

## Results

32 correlations, N = 10M samples, `g++ -O1`. The last column is the number of
instructions touching the stack inside the function, counted straight out of
the generated assembly.

| accumulators per loop | loops | time | speedup | stack refs | of which XMM |
|---:|---:|---:|---:|---:|---:|
| 32 | 1 | 237.9 ms | 1.00x | **134** | 90 |
| 16 | 2 | 92.0 ms | 2.59x | **57** | 12 |
| **8** | **4** | **55.6 ms** | **4.28x** | **0** | 0 |
| 4 | 8 | 53.1 ms | 4.48x | 0 | 0 |

The `XMM` column separates spilled floating-point accumulators from spilled
general-purpose registers (the reference pointers). Both cost a load and a
store per iteration; the accumulators are the ones the transformation is aimed
at.

## Measuring the spills

```bash
g++ -O1 -S -o fission.s loop_fission.cpp
for f in fused split16 split8 split4; do
    echo -n "$f "
    sed -n "/correlate_$f/,/^\s*\.size/p" fission.s | grep -c '(%rsp)'
done
```

The spilled accumulators are plainly visible in the 32-accumulator version —
written to a stack slot and read back from it inside the hot loop:

```
    movss   %xmm1, -52(%rsp)     # spill an accumulator
    ...
    addss   -52(%rsp), %xmm15    # read it back to accumulate into it
```

In the 8- and 4-accumulator versions there are no `(%rsp)` references in the
loops at all. Every accumulator lives in an XMM register for the duration.

Note that the function boundary matters when counting: the last correlation
function in the file runs to the end of the translation unit unless you stop at
its `.size` directive, which is why the `sed` range above ends there.

## Analysis

### The knee is where the spills reach zero

The timing and the spill count are the same curve. Going from 32 to 16
accumulators removes 77 of the 134 stack references — 78 of the 90 XMM spills —
and roughly halves the time. Going from 16 to 8 removes the remaining 57 and
takes another 40% off.
Going from 8 to 4 removes nothing, because there was nothing left, and buys
nothing measurable.

That is as direct a confirmation of the mechanism as this kind of example gets:
the speedup is not a plausible story about caches, it is the elimination of a
counted number of instructions.

### Sixteen accumulators is not enough here

The natural guess is to split 32 in half. It leaves **57 stack references** and
most of the available speedup on the table. The breakdown is instructive: only
12 of those are accumulator spills, but 45 are spilled reference pointers.
Sixteen accumulators nearly fit the 16 XMM registers — and in doing so they
squeeze the 16 pointers out of the general-purpose registers instead.

On an ISA with 32 FP registers, 16 per loop would fit and the half-split would
be right. **The register count is an ISA property, so the best fission factor
is not portable** — re-measure it whenever you change targets.

### Why splitting further stops paying

At 4 accumulators per loop there are no spills left to remove, and the loop now
makes eight passes over a 40 MB signal instead of four. The extra memory
traffic roughly cancels what little is left to gain — 53.1 ms against 55.6 ms
is within measurement noise of each other. Past the knee, fission is just extra
passes.

### A measurement trap in this example

The 32 reference vectors are 32 separate 40 MB allocations, and `malloc` hands
them back with identical alignment. That makes the streams congruent in the
cache and in the DRAM banks. Staggering the allocations by a cache line each is
worth a large fraction of the one-loop time on its own and has nothing to do
with fission. The times above use the natural allocation, so the unfissioned
case carries some of that penalty too — which is worth knowing before quoting
the 4.28x as purely a register-allocation result.

## Key takeaways

1. **Fission trades memory passes for register pressure.** It wins while there
   are spills on the critical path and stops the moment there aren't.

2. **Count the spills; don't guess at them.** `grep -c '(%rsp)'` on the
   generated assembly turns the whole question into an observation. Here the
   speedup tracks the count exactly.

3. **The right split width is an ISA property.** 8 accumulators per loop on
   x86-64's 16 XMM registers; 16 would be right on a machine with 32.

4. **Accumulate into local variables, not into the output array.** Otherwise
   pointer aliasing keeps everything in memory, you get no spills and no
   speedup from fission, and the profile misleads you about why.
