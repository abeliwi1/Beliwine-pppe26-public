CC     = clang
CFLAGS = -Xpreprocessor -fopenmp -O3 \
         -I/opt/homebrew/opt/libomp/include
LDFLAGS = -L/opt/homebrew/opt/libomp/lib -lomp

BINS = hello_omp fib vector_add reduction dot_product

all: stencil $(BINS) omp_prefix_sum

$(BINS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

stencil: stencil/stencil.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o stencil/stencil

omp_prefix_sum: omp_scheduling_prefix_sum/omp_prefix_sum.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o omp_scheduling_prefix_sum/$@

clean:
	rm -f stencil/stencil $(BINS) omp_scheduling_prefix_sum/omp_prefix_sum
