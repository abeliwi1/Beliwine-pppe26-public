# Parallel prefix sum: OpenMP and Cilk versions of the three-pass block scan.
#   make            build both
#   make clean

CC      = clang
OMPINC  = -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include   # macOS, Homebrew libomp
OMPLIB  = -L/opt/homebrew/opt/libomp/lib -lomp
CILKCC  = $(HOME)/opencilk/bin/clang

all: omp_prefix_sum cilk_prefix_sum

omp_prefix_sum: omp_prefix_sum.c
	$(CC) -O3 $(OMPINC) $< -o $@ $(OMPLIB) -lm

cilk_prefix_sum: cilk_prefix_sum.c
	$(CILKCC) -fopencilk -O3 $< -o $@ -lm

clean:
	rm -f omp_prefix_sum cilk_prefix_sum

.PHONY: all clean
