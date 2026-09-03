`git publish` is an alias to update main to pppe26-main.

## RB Thoughts

There are really only 2 computations in all of CS.
  * sorting
  * matrix multiplication

* start with speedup
  * loop optimizations as examples of speedup.
* then pipelining note that these are processor things
    * and pipeline examples
* then let's talk about compilers
  * and do code generation
  * even last year it was generate and read assembly
  * now AI can tell you a lot about what's going on
* then ILP/CPI note that these are processor ops
  * out of order and speculative execution
* now that we have a deep undertand of regular, serial code let's do sorting

When we do parallelism.  Let's do make it go fast (openmp, and cilk). then let's do scheduling. then let's do Amdahl's later.

# Structure

* Intro (Currently Lec 0)
  * what simple example to DO???



* Amdahl's law and speedup (Lec 2)
* Looping unrolling as first form of optimization
  * think about the processor instructions, overhead, and branches 
    * unrolling (1 whole lecture)
    * loop fusion

* Compiler and optimizations levels
  * 02 -- auto vectorization, why we run all stuf at 01
  * strength reduction as something compiler does

* Branches
   * TODO something demonstrating how branch predication works.

* CPU stuff to realize ILP: CPI
  * OOO and speculative execution

* Pipelining -- 

* Cache hierarchy
  * loop interchange and tiling blocking. loop fission.
  * false sharing.

* Roofline -- before we do parallelism -- understand using the processor well.

* Joblib -- concept of parallelism
  * hazard of choosing to parallelize bad imnplementataions
  * roofline is our guide.

### Multicore Parallelism

OpenMP block parallelism -- prefix sum

Cilk and work stealing -- sparse_col_sum
  * parallel for
  * work stealing
  * dynamic OpenMP scheduling

Compare and contrast of OpenMP and Cilk.



### Stuff to cover from Claude

** Loop Optimizations **
Loop unrolling
Loop fusion
Loop fission/distribution
Loop interchange
Loop tiling/blocking


** Software pipelining **

Data Dependency Reduction
Multiple accumulators
Temporary variables
Strength reduction
Common subexpression elimination
Dead code elimination

** Memory Optimizations **

Data prefetching
Cache blocking
Structure of Arrays (SoA) vs Array of Structures (AoS). TODO
Memory alignment. TODO
Avoiding false sharing
Reducing pointer aliasing (restrict keyword). TODO

** Branch Optimizations **
Branch-free code (using arithmetic/bitwise ops)
Likely/unlikely hints
Lookup tables instead of branches
Predicated execution
Loop unswitching

** Instruction Scheduling **
Instruction reordering. (part of piplelining)
Separating dependent instructions (in ILP)
Interleaving independent operations (part of separating dependent instructions1)  -- this is redundant with loop fusion and other stuff.

## Stuff in MIT not here suggestions

Matrix multiplication as a worked example — invoked as a motif in TODO.md but has no dedicated directory (sorting/ has real depth; matrix multiply doesn't)
Bentley Rules for Optimizing Work — general algorithmic/data-structure work-reduction techniques (precomputation, caching, sparsity)
Bit hacks — beyond the branch-free/bitwise tricks noted in branch_optimizations/, no dedicated bit-manipulation coverage
Assembly language & computer architecture / C-to-assembly — no dedicated lecture (TODO.md notes this used to be covered by reading generated assembly but that's being reconsidered)
Work-span analysis of multithreaded algorithms — the theory side of parallelism (races, determinacy, Brent's theorem) isn't present; the repo is Amdahl's-law-centric, not work/span-centric
Measurement and timing methodology — no dedicated lecture on profiling rigor (perf, statistical timing pitfalls)
(Parallel) storage allocation — no coverage of malloc internals or custom/parallel allocators
Cache-oblivious algorithms — memory_hierarchy/ covers cache-aware blocking, not the cache-oblivious algorithm design style
Nondeterministic parallel programming / reducers — races and determinacy aren't addressed
Lock-free synchronization — false_sharing/ touches contention, but not lock-free/wait-free algorithm design
Domain-specific languages and autotuning
Graph optimization — no graph-algorithm performance content
High performance in dynamic languages — joblib/ is Python-based parallelism, but nothing on dynamic-language performance specifically (JIT, interpreter overhead, etc.)