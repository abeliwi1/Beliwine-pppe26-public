# Activity 1 — Pipeline Stalls: Independent Accumulators Written Responses

## Part 2

### Level -O0
One chain (stalled):          152 ms\
Four chains (pipelined):      101 ms\
Speedup: 1.50x

### Level -O1
One chain (stalled):           68 ms\
Four chains (pipelined):       18 ms\
Speedup: 3.78x

### Level -O2
One chain (stalled):           68 ms\
Four chains (pipelined):       17 ms\
Speedup: 4.00x

### Level -O3
One chain (stalled):           68 ms\
Four chains (pipelined):       17 ms\
Speedup: 4.00x


## Part 3

### (a)
No, the core noTempVars loop (labeled LBB0_1 in -O1, -O2, and -O3) is completely identical across all three optimization levels. The compiler never attempts to change the mathematical structure of the loop.

Evidence: 
'diff' between -O1/-O2 -->
diff -s <(sed -n '3,31p' activity1_O1.s) <(sed -n '3,31p' activity1_O2.s) // COMPARING LINES OF ASM CODE CORRESPONDING TO FUNCTION
Files /dev/fd/11 and /dev/fd/12 are identical // NO DIFFERENCE

'diff' between -O1/-O3 -->
diff -s <(sed -n '3,31p' activity1_O1.s) <(sed -n '3,31p' activity1_O3.s) 
Files /dev/fd/11 and /dev/fd/12 are identical // NO DIFFERENCE

### (b)
Similarly, the core withTempVars loop (labeled LBB1_1 in -O1, -O2, and -O3) is completely identical across all three optimization levels. It establishes four separate registers and maintains that exact same strategy in -O1, -O2, and -O3. The compiler does not attempt to unroll it further.

Evidence:
'diff' between -O1/-O2 -->
diff -s <(sed -n '32,66p' activity1_O1.s) <(sed -n '32,66p' activity1_O2.s) // COMPARING LINES OF ASM CODE CORRESPONDING TO LOOP
Files /dev/fd/11 and /dev/fd/12 are identical // NO DIFFERENCE

'diff' between -O1/-O3 -->
diff -s <(sed -n '32,66p' activity1_O1.s) <(sed -n '32,66p' activity1_O3.s) 
Files /dev/fd/11 and /dev/fd/12 are identical // NO DIFFERENCE

### (c)
No, it's clear that neither function uses vector/SIMD registers for the math. You will only see scalar w (32-bit) registers for loading the integer data from memory, and scalar x (64-bit) registers for the logical OR and multiplication. While compilers love to auto-vectorize standard additions, 64-bit integer multiplication is often too expensive or unsupported in basic vector units to be profitable, so LLVM keeps it strictly in scalar registers.

Evidence:
If you look at the loop bodies for noTempVars (label LBB0_1) and withTempVars (label LBB1_1) across all three optimization levels, the compiler exclusively uses scalar x registers for the math.
- In noTempVars, the multiplies are done using scalar registers like x0, x11, and x12 (mul x11, x11, x12).
- In withTempVars, it uses scalar registers like x8, x12, x13, and x14 (mul x12, x12, x16).

### (d)
In noTempVars, the assembly looks like this (Look at LBB0_1):
```asm
mul x11, x0, x11    // writes x11
orr x12, x12, #0x1
mul x11, x11, x12   // reads x11, writes x11
ldpsw x12, x13, [x8], #16
orr x12, x12, #0x1
mul x11, x11, x12   // reads x11, writes x11
orr x12, x13, #0x1
mul x0, x11, x12    // reads x11, writes x0 (x0 becomes the input for the next loop)
```
And yes, each mul instruction undeniably reads the exact register the previous mul just wrote here. This is a severe Read-After-Write (RAW) hazard causing a pipeline stall.

In withTempVars: (Look at LBB1_1)
```asm
mul x8, x8, x15     // updates x8
mul x12, x12, x16   // updates x12
mul x13, x13, x17   // updates x13
mul x14, x14, x0    // updates x14
```
The destination registers are perfectly independent. The CPU can send all four of these to its ALU execution ports simultaneously.

Instruction count: The loop bodies for noTempVars (LBB0_1) and withTempVars (LBB1_1) both execute 13 instructions. 
As a result, withTempVars actually executes the same number of instructions per loop iteration as noTempVars, yet it runs roughly 4x faster (as measured in Part 2). This definitively proves that the speedup comes purely from Instruction Level Parallelism (ILP) allowing the CPU to overlap work, not from executing less work per iteration.

### (e)
No, the compiler did not split the chain, even at -O3. The numbers in Part 2 and the assembly in Part 3 prove this. While unsigned integer multiplication is strictly associative and commutative (meaning the compiler is mathematically allowed to do this transformation safely), compilers are deeply conservative about re-associating integer operations. They lack the aggressive heuristics to automatically split integer reduction chains like this without explicit #pragma hints or manual developer intervention.
Takeaway: You cannot blindly rely on the compiler's "magic" to fix architectural bottlenecks. If your code creates a strict data dependency chain, the compiler will usually compile exactly that chain, stalls and all.

Evidence:
Looking at the loop bodies for noTempVars (label LBB0_1) across all three optimization levels, the instructions are identical bit-for-bit. The compiler retained the exact same severe Read-After-Write (RAW) hazard (x11 -> x11 -> x11 -> x0) despite being at maximum optimization -- shown in part 2 results as well. My Part 2 numbers show that the execution time for the 'One chain' function stays essentially constant across -O1, -O2, and -O3. If the compiler had automatically re-associated and split the chain, the noTempVars time at -O3 would have dropped to match the withTempVars time.



## AI Disclosure

Tool Used: Gemini (Google AI)

How I used it:

Implementation (Part 1): I used the AI to help brainstorm and structure the withTempVars function, specifically using four independent accumulators (x0–x3) to split the loop into distinct load and compute phases and break the Read-After-Write (RAW) data hazard.

Assembly Analysis (Part 3): I fed my compiler-generated assembly into the model to help locate and analyze the loops/instructions within (LBB0_1 and LBB1_1) for each file, and verify that neither reduction loop utilized vector/SIMD registers (noting that SIMD was only used in _main for array initialization).

Tracing Dependencies (Part 3): I used the AI to trace register operands across the mul instructions to verify that noTempVars repeatedly read and wrote the same accumulator register, whereas withTempVars dispatched across four distinct registers.

Instruction Verification & Collaboration (Part 3): I collaboratively reviewed the assembly instruction counts with the AI, identifying and correcting an initial counting discrepancy and verifying that the final 3 mul instructions in %bb.2 belonged to the post-loop combination step rather than the inner loop body.

Conceptual Understanding: I consulted the model to explain the architectural mechanics behind Instruction-Level Parallelism (ILP) in regards to this pipelining activity example, latency hiding, and why the compiler’s optimization passes do not automatically re-associate integer multiplications.