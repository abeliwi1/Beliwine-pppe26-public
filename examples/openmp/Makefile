CC     = clang
CFLAGS = -Xpreprocessor -fopenmp -O3 \
         -I/opt/homebrew/opt/libomp/include
LDFLAGS = -L/opt/homebrew/opt/libomp/lib -lomp

BINS = hello_omp fib vector_add reduction dot_product

all: $(BINS)

$(BINS): %: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f $(BINS)
