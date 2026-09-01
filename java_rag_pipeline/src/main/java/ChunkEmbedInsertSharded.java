import dev.langchain4j.data.document.Document;
import dev.langchain4j.data.document.DocumentSplitter;
import dev.langchain4j.data.document.DocumentTransformer;
import dev.langchain4j.data.document.loader.FileSystemDocumentLoader;
import dev.langchain4j.data.document.splitter.DocumentSplitters;
import dev.langchain4j.data.embedding.Embedding;
import dev.langchain4j.data.segment.TextSegment;
import dev.langchain4j.model.embedding.EmbeddingModel;
import dev.langchain4j.store.embedding.EmbeddingMatch;
import dev.langchain4j.store.embedding.EmbeddingSearchRequest;
import dev.langchain4j.store.embedding.EmbeddingSearchResult;
import dev.langchain4j.store.embedding.inmemory.InMemoryEmbeddingStore;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.regex.Pattern;
import java.util.stream.Stream;

/*
 * ChunkEmbedInsertSharded.java -- fix the straggler problem at task-creation
 * time instead of at scheduling time.
 *
 * ChunkEmbedInsertPerFile.java gives each of the 10 books its own thread,
 * which parallelizes chunk and embed together but inherits the books'
 * own size skew: les_miserables.txt (3.4MB) keeps its thread busy ~10x
 * longer than the_time_machine.txt (204KB), so most threads sit idle
 * waiting on the two or three biggest books. See the "Straggler Threads"
 * chart from this investigation for the measured breakdown.
 *
 * The fix here doesn't need work stealing or ForkJoinPool: it needs the
 * *tasks* to be roughly even-sized before any pool ever sees them. Each
 * file's cleaned text is cut into ~SHARD_CHARS-sized pieces -- snapped to
 * the nearest paragraph break so no cut lands mid-sentence -- and every
 * shard becomes its own independent (chunk, embed) task. A small book
 * stays one shard; les_miserables.txt becomes ~7. That turns 10 wildly
 * uneven tasks into ~30-40 comparably-sized ones, submitted to a plain
 * fixed thread pool. A thread that finishes a small book's one shard
 * just pulls the next queued shard -- possibly another piece of
 * les_miserables.txt that a different thread already started on. Same
 * load-balancing outcome as stealing, through the boring shared-queue
 * mechanism ChunkEmbedInsertPool.java already used, because the fix
 * happened one step earlier, at sharding time.
 *
 * SHARD_CHARS = 500,000 chars (~500KB): small enough that even the
 * biggest book (les_miserables.txt, ~3.4MB) splits into 7 shards --
 * enough oversubscription over 10 threads for the queue to actually
 * rebalance -- while staying well above per-task dispatch overhead
 * (~500KB of text is tens of milliseconds of real chunk+embed work).
 *
 * Pool size defaults to availableProcessors(), overridable with
 * -Dthreads=N -- e.g. to cap it at 4 to match a 4-performance-core subset
 * of a mixed P+E machine, isolating skew's effect from core heterogeneity.
 *
 * Usage:
 *   mvn compile exec:java -Dexec.mainClass=ChunkEmbedInsertSharded -Dexec.args="corpus"
 *   java -Dthreads=4 -cp ... ChunkEmbedInsertSharded corpus4
 */
public class ChunkEmbedInsertSharded {
    private static final int TRIALS = 5;
    private static final int SHARD_CHARS = 500_000;
    private static final Pattern GUTENBERG_HEADER =
            Pattern.compile("(?s)^.*?\\*\\*\\* START OF[^\\n]*\\*\\*\\*\\s*");
    private static final Pattern GUTENBERG_FOOTER =
            Pattern.compile("(?s)\\*\\*\\* END OF[^\\n]*\\*\\*\\*.*$");

    private record ShardResult(List<TextSegment> segments, List<Embedding> embeddings) {}

    public static void main(String[] args) throws InterruptedException, ExecutionException, IOException {
        if (args.length < 1) {
            System.err.println("usage: ChunkEmbedInsertSharded <directory> [query]");
            System.exit(1);
        }
        Path dir = Paths.get(args[0]);
        String query = args.length > 1 ? args[1] : null;

        DocumentTransformer stripBoilerplate = document -> {
            String text = GUTENBERG_FOOTER.matcher(
                    GUTENBERG_HEADER.matcher(document.text()).replaceFirst("")).replaceFirst("");
            return Document.from(text.trim(), document.metadata());
        };
        DocumentSplitter splitter = DocumentSplitters.recursive(800, 100);
        EmbeddingModel model = new HashEmbeddingModel();
        InMemoryEmbeddingStore<TextSegment> store = new InMemoryEmbeddingStore<>();

        List<Path> files;
        try (Stream<Path> paths = Files.list(dir)) {
            files = paths.filter(Files::isRegularFile).sorted().toList();
        }

        int numThreads = Integer.getInteger("threads", Runtime.getRuntime().availableProcessors());
        ExecutorService pool = Executors.newFixedThreadPool(numThreads);

        List<TextSegment> allSegments = null;
        List<Embedding> allEmbeddings = null;

        for (int trial = 1; trial <= TRIALS; trial++) {
            long t0 = System.nanoTime();

            List<Document> shards = new ArrayList<>();
            for (Path file : files) {
                Document cleaned = stripBoilerplate.transform(FileSystemDocumentLoader.loadDocument(file));
                shards.addAll(shard(cleaned));
            }

            List<Future<ShardResult>> futures = new ArrayList<>(shards.size());
            for (Document shardDoc : shards) {
                futures.add(pool.submit(() -> {
                    List<TextSegment> segments = splitter.split(shardDoc);
                    List<Embedding> embeddings = model.embedAll(segments).content();
                    return new ShardResult(segments, embeddings);
                }));
            }

            allSegments = new ArrayList<>();
            allEmbeddings = new ArrayList<>();
            for (Future<ShardResult> future : futures) {
                ShardResult result = future.get();
                allSegments.addAll(result.segments());
                allEmbeddings.addAll(result.embeddings());
            }

            long totalNanos = System.nanoTime() - t0;
            System.out.printf("trial %d/%d  %2d files  %3d shards  %6d segments  %8.3f s  (pool of %d threads, chunk+embed)%n",
                    trial, TRIALS, files.size(), shards.size(), allSegments.size(), totalNanos / 1e9, numThreads);
        }
        pool.shutdown();

        long t3 = System.nanoTime();
        store.addAll(allEmbeddings, allSegments);
        long updateNanos = System.nanoTime() - t3;

        Path outFile = dir.resolveSibling(dir.getFileName() + "_embeddings_store.json");
        store.serializeToFile(outFile);

        System.out.printf("%-8s %6d entries    %8.3f s%n", "update", allSegments.size(), updateNanos / 1e9);
        System.out.println("wrote vector store to " + outFile);

        if (query != null) {
            Embedding queryEmbedding = model.embed(query).content();
            EmbeddingSearchRequest request = EmbeddingSearchRequest.builder()
                    .queryEmbedding(queryEmbedding)
                    .maxResults(3)
                    .build();
            EmbeddingSearchResult<TextSegment> result = store.search(request);

            System.out.println("\nnearest chunks to \"" + query + "\":");
            for (EmbeddingMatch<TextSegment> match : result.matches()) {
                String fileName = match.embedded().metadata().getString(Document.FILE_NAME);
                String snippet = match.embedded().text().substring(0, Math.min(120, match.embedded().text().length()));
                System.out.printf("  %.4f  %-25s %s...%n", match.score(), fileName, snippet.replace("\n", " "));
            }
        }
    }

    /** Cuts a document's text into ~SHARD_CHARS-sized pieces, each cut snapped to the nearest paragraph break. */
    private static List<Document> shard(Document document) {
        String text = document.text();
        int numShards = Math.max(1, (int) Math.ceil((double) text.length() / SHARD_CHARS));
        if (numShards == 1) {
            return List.of(document);
        }

        List<Document> shards = new ArrayList<>(numShards);
        int start = 0;
        for (int i = 1; i <= numShards; i++) {
            int end = (i == numShards) ? text.length() : snapToParagraphBreak(text, (int) ((long) text.length() * i / numShards));
            shards.add(Document.from(text.substring(start, end), document.metadata().copy()));
            start = end;
        }
        return shards;
    }

    /** Finds the "\n\n" nearest to idealEnd within a small search window, falling back to idealEnd if none is close. */
    private static int snapToParagraphBreak(String text, int idealEnd) {
        int searchRadius = 2000;
        int lo = Math.max(0, idealEnd - searchRadius);
        int hi = Math.min(text.length(), idealEnd + searchRadius);

        int best = idealEnd;
        int bestDistance = Integer.MAX_VALUE;
        for (int i = text.indexOf("\n\n", lo); i != -1 && i <= hi; i = text.indexOf("\n\n", i + 2)) {
            int distance = Math.abs(i - idealEnd);
            if (distance < bestDistance) {
                bestDistance = distance;
                best = i + 2;
            }
        }
        return best;
    }
}
