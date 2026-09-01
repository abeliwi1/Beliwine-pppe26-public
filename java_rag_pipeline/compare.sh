#!/bin/bash
# compare.sh -- run the serial and per-file variants against corpus/ and
# log results to comparison_results.txt. Meant to be run directly in a
# real terminal (not through Claude Code's sandboxed tool execution),
# since the whole point is to measure true multicore scaling on
# unthrottled hardware.
#
# Runs the compiled classes directly via `java -cp`, NOT `mvn exec:java`.
# exec:java runs your main() inside Maven's own JVM via a background
# thread + reflection, and empirically that adds enough overhead/
# contention to flatten out real multicore scaling -- every threaded
# variant looked identical to serial until this was discovered.
set -e
cd "$(dirname "$0")"

OUT=comparison_results.txt
VARIANTS=(ChunkEmbedInsert ChunkEmbedInsertPerFile ChunkEmbedInsertSharded)

echo "java rag pipeline: serial vs per-file vs sharded" > "$OUT"
echo "date: $(date)" >> "$OUT"
echo "cores (sysctl hw.ncpu / nproc): $(sysctl -n hw.ncpu 2>/dev/null || nproc)" >> "$OUT"
echo "" >> "$OUT"

echo "compiling (clean)..."
mvn -q clean compile

echo "resolving classpath..."
CP=$(mvn -q dependency:build-classpath -Dmdep.outputFile=/dev/stdout 2>/dev/null | tail -1)

# Each program loops its own embed stage TRIALS times internally, so one
# process invocation per variant is enough.
for CLASS in "${VARIANTS[@]}"; do
    echo "running $CLASS..."
    echo "=== $CLASS ===" >> "$OUT"
    java -cp "target/classes:$CP" "$CLASS" corpus 2>&1 | grep -v SLF4J >> "$OUT"
    echo "" >> "$OUT"
done

echo "done. results written to $OUT"
