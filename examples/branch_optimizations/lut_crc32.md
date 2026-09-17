# Lookup Tables: Precomputation the Compiler Cannot Do

## Source

[lut_crc32.cpp](lut_crc32.cpp)

## What this shows

A 256-entry table replaces eight rounds of data-dependent shift/xor per byte in
a CRC32 computation, and runs **3.34x faster**. The interesting part is not the
number — it is that the number **does not move between `-O1` and `-O3`**. The
compiler has nothing to contribute, and this write-up is mostly about why.

## The two versions

**Bitwise** — eight data-dependent rounds per byte:

```cpp
uint32_t c = 0xFFFFFFFFu;
for (int i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++)
        c = (c & 1) ? (POLY ^ (c >> 1)) : (c >> 1);
}
```

**Table-driven** — the eight rounds collapse into one indexed load:

```cpp
uint32_t c = 0xFFFFFFFFu;
for (int i = 0; i < n; i++)
    c = TABLE[(c ^ d[i]) & 0xFF] ^ (c >> 8);
```

`TABLE[i]` holds the result of running the inner loop to completion for byte
value `i`. Building all 256 entries costs 2048 rounds, paid once at startup.

## Results (Apple M5, N=32M bytes, Apple Clang 17)

| level | bitwise | table | speedup |
|-------|---------|-------|---------|
| `-O0` | 1106.7 ms | 58.3 ms | 18.98x |
| `-O1` | 189.7 ms | 56.5 ms | **3.36x** |
| `-O2` | 188.9 ms | 56.6 ms | **3.34x** |
| `-O3` | 189.4 ms | 56.7 ms | **3.34x** |

Three optimization levels, three identical ratios. The bitwise version does not
improve by one percent from `-O1` to `-O3`.

(The `-O0` row says only that unoptimized code is slow — the 19x there is stack
traffic the optimizer removes, and it teaches nothing about the technique.)

---

## Why the compiler cannot do this

It is worth being precise, because "the compiler can't optimize that" is
usually a claim about *effort* and this one is a claim about *kind*. Four
separate things stand in the way, and each alone would be enough.

### 1. It requires discovering a theorem about CRC arithmetic

The substitution is valid because of a specific mathematical property. Look at
what the eight rounds do: each round inspects bit 0 and shifts right by one. So
after eight rounds the low eight bits have been shifted out of the register
entirely, and the high 24 bits have simply moved down by eight.

The contribution those eight departing bits make to the final value is a
function of **those eight bits alone** — and because CRC arithmetic is linear
over GF(2), that contribution is a fixed XOR mask. Hence:

```
c_new  =  (c >> 8)  ^  TABLE[c & 0xFF]
          └───┬───┘     └──────┬─────┘
       the high 24 bits    the effect of the
       just shifting        8 departing bits,
                            precomputed
```

That is a small theorem. A compiler would have to *prove* it — establish that
the loop is linear over GF(2), that the register's low byte fully determines the
eight rounds' contribution, and that tabulating that contribution preserves
semantics for all inputs. Optimizers do not do algebra over finite fields. They
pattern-match and they do dataflow analysis. Nothing in `-O3` is in the business
of discovering identities specific to polynomial division.

### 2. It requires creating a new program phase

Even granting the theorem, the rewrite is not local. The table has to *exist*
before the loop runs, which means inventing an initialization phase that
executes 2048 rounds at startup and writing 1 KB of results somewhere.

Compiler optimizations rewrite code in place. They delete instructions, reorder
them, widen them, unroll them, hoist them out of loops. None of those operations
creates a new computation that runs at a different time from the code it
replaced. A transformation that says "do this other work earlier, store the
answers, then look them up" is restructuring the program, not optimizing a loop.

### 3. It is a space-time tradeoff, and the compiler has no basis to make it

The table costs 1 KB of memory, permanently. Whether that is acceptable is not a
question about the code — it depends on the target, the cache budget, how many
other tables the program already has, and whether this is a microcontroller or a
server.

Compilers make tradeoffs when they can bound them (inlining budgets, unroll
factors) and refuse when they cannot. There is no principled amount of memory an
optimizer may spend on your behalf to make a loop faster, so it spends none.

### 4. The loop is serial and data-dependent, so the usual tools do not apply

Every round reads the `c` the previous round wrote. That rules out the two
things an optimizer would normally reach for:

* **Vectorization** — there is no parallelism to extract. Each round depends on
  the last, and each byte depends on the previous byte's final CRC.
* **Unrolling for work reduction** — unrolling removes *loop overhead*, not
  work. The compiler does unroll the inner loop fully (see below), and it buys
  nothing, because the eight rounds still have to happen.

The only way to make this loop faster is to not do the rounds. That is an
algorithmic change, and algorithmic changes are the programmer's job.

---

## What the compiler *does* do — it is not idle

At `-O2` the bitwise inner loop is fully unrolled and completely branchless:

```asm
LBB0_2:
    ldrb w11, [x0], #1
    eor  w10, w10, w11
    lsr  w11, w10, #1
    eor  w12, w11, w8
    tst  w10, #0x1
    csel w10, w11, w12, eq     ; round 1 — no branch
    lsr  w11, w10, #1
    eor  w12, w11, w8
    tst  w10, #0x1
    csel w10, w11, w12, eq     ; round 2
    ...                        ; ... through round 8
```

The compiler applied everything it had: it unrolled the `k`-loop eight times and
if-converted every ternary into a `csel`. The result is 36 straight-line
instructions per byte with no branches and no stalls to speak of — genuinely
good code for the algorithm it was given.

It is still 3.34x slower than not doing the work.

| version | inner-loop instructions per byte |
|---------|----------------------------------|
| bitwise | **36** — 8 rounds x (`lsr`, `eor`, `tst`, `csel`), plus load and fold |
| table | **7** — `ldrb`, `eor`, `and`, `ldr`, `eor`, `subs`, `b.ne` |

5.1x fewer instructions for a 3.34x speedup; the shortfall is the table's
data-dependent load, which is L1-resident but not free.

## A note on branches

The ternary `(c & 1) ? ... : ...` branches on the low bit of a CRC register —
about as unpredictable as a branch can be. It costs nothing, because the
compiler if-converts it to `csel` before it ever executes.

That is why **this example uses no compiler barrier**. Both versions are already
branchless, and everything measured here is work, not branch prediction. If you
came here expecting a misprediction story, it is in
[branch_free.md](branch_free.md) instead.

## Where else this pattern applies

The family is "tables whose entries are the output of a real computation":

* **Checksums and hashes** — CRC variants, Adler, table-driven Rabin-Karp.
* **Transcendentals** — `sin`, `exp`, `log` over a bounded domain, where a table
  plus interpolation replaces a polynomial evaluation.
* **Colour and gamma** — sRGB encode/decode is `powf` per channel, or a
  256-entry table.
* **Bit manipulation** — population count, leading-zero count, bit reversal on
  hardware without dedicated instructions.

The common shape: the entry costs many operations to compute, the domain is
small enough to enumerate, and the result is fixed for the life of the program.

## Key takeaways

1. **Tabulate computations, not expressions.** The test is how much work one
   table entry represents. Eight rounds of shift/xor: worth a table. A couple of
   arithmetic operations: the compiler already has a peephole for it, and the
   table will lose — an indexed load is opaque to the vectorizer where
   arithmetic is not.

2. **A flat curve across `-O` levels is the signature of a real optimization.**
   When `-O1`, `-O2`, and `-O3` give you the same ratio, the compiler has
   nothing to contribute and the transformation is genuinely yours. When the
   ratio collapses at `-O2`, the compiler has found your trick and you are
   measuring your own redundancy.

3. **Compilers optimize; they do not restructure.** Deleting, reordering,
   widening, unrolling — all in place, all preserving when things happen. Moving
   work to an earlier phase and storing the results is a different category of
   change, and it stays with the programmer.

4. **"The compiler can't do this" should name a reason.** Here there are four:
   an unproven theorem, a new program phase, an unbounded space-time tradeoff,
   and a serial dependency. A claim that cannot name one is usually just a claim
   that nobody checked the assembly.
