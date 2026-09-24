# Benchmarks behind cache_hierarchy.ipynb.
#
#   make            build everything
#   make data       run every benchmark and refresh the .csv files
#
# Each program pins itself to a CPU and re-measures the core clock while it
# runs; see the comments at the top of each .c file for why that is necessary
# on a mobile part whose clock moves between 0.6 and 5.1 GHz.

CC      = gcc
CFLAGS  = -O2 -Wall
FAST    = -O3 -march=native -Wall
SCRATCH = /tmp/cache_hierarchy_scratch.bin

# every program shares the timing harness; changing it rebuilds all of them
HDR  = harness.h

BINS = cache_latency stride_bandwidth core_to_core row_column bandwidth_scaling \
       storage_latency spin_cost set_conflict tlb_walk vector_width

all: $(BINS)

cache_latency: cache_latency.c $(HDR)
	$(CC) $(CFLAGS) -o $@ $<

# -fno-tree-vectorize: GCC vectorizes the row-order loop at -O2 and the
# row-vs-column comparison stops being about access order alone.
row_column: row_column.c $(HDR)
	$(CC) $(CFLAGS) -fno-tree-vectorize -o $@ $<

storage_latency: storage_latency.c $(HDR)
	$(CC) $(CFLAGS) -o $@ $<

# Supporting measurements for the two HTML pages rather than the notebook.
spin_cost: spin_cost.c $(HDR)
	$(CC) $(CFLAGS) -o $@ $<

set_conflict: set_conflict.c $(HDR)
	$(CC) $(CFLAGS) -fno-tree-vectorize -o $@ $<

tlb_walk: tlb_walk.c $(HDR)
	$(CC) $(CFLAGS) -o $@ $<

# -march=native for the AVX-512 intrinsics
vector_width: vector_width.c $(HDR)
	$(CC) $(CFLAGS) -march=native -o $@ $<

stride_bandwidth: stride_bandwidth.c $(HDR)
	$(CC) $(FAST) -o $@ $<

core_to_core: core_to_core.c $(HDR)
	$(CC) $(CFLAGS) -pthread -o $@ $<

bandwidth_scaling: bandwidth_scaling.c $(HDR)
	$(CC) $(FAST) -pthread -o $@ $<

# Roughly 12 minutes total.  Run on an otherwise idle machine: every one of
# these measures a shared resource.
data: all
	./cache_latency 0     > latency.csv
	./stride_bandwidth 0  > stride_bandwidth.csv
	./row_column 0        > row_column.csv
	./bandwidth_scaling   > bandwidth_scaling.csv
	./core_to_core 12     > core_to_core.csv
	./storage_latency $(SCRATCH) 2048 > storage.csv
	{ ./spin_cost 0; echo; ./spin_cost 5 | head -6; echo; ./set_conflict; } > spin_and_sets.txt
	./tlb_walk 0          > tlb_walk.csv
	./vector_width 0      > vector_width.csv

# 1_core_cache.html is generated from the .csv files rather than hand-authored,
# so a re-measurement does not mean editing SVG coordinates by hand.
page: 1_core_cache.html block_diagram.html

1_core_cache.html: make_1core_page.py latency.csv stride_bandwidth.csv \
                   vector_width.csv row_column.csv
	python3 make_1core_page.py --standalone > $@

# reads the cache geometry from /sys and reuses 1_core_cache.html's stylesheet
block_diagram.html: make_block_diagram.py latency.csv 1_core_cache.html
	python3 make_block_diagram.py --standalone > $@

clean:
	rm -f $(BINS)

.PHONY: all data page clean
