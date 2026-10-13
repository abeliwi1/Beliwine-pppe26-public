CC     = gcc
CFLAGS = -O2 -Wall

all: fork_workers omp_matmul

fork_workers: fork_workers.c
	$(CC) $(CFLAGS) -o $@ $<

omp_matmul: omp_matmul.c
	$(CC) $(CFLAGS) -fopenmp -o $@ $<

clean:
	rm -f fork_workers omp_matmul
