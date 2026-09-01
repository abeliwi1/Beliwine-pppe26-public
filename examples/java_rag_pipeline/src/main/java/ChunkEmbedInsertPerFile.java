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
import java.util.regex.Pattern;
import java.util.stream.Stream;

/*
 * ChunkEmbedInsertPerFile.java -- one thread per FILE, not per chunk.
 *
 * ChunkEmbedInsert.java loads and splits all documents up front (a single
 * bulk read + bulk chunk pass), then only parallelizes the embed stage.
 * That leaves "chunk" as the biggest cost by far (~0.55s serial, versus
 * a few hundredths of a second for a threaded embed) -- and it never gets
 * parallelized, because it runs once, before embed's threads start.
 *
 * This version instead gives each FILE its own thread, and that thread
 * does the whole pipeline for its file: load, strip boilerplate, split
 * into segments, embed them. With ~10 books that's ~10 threads, each
 * doing a meaningfully large, self-contained unit of work -- so this
 * parallelizes chunk and embed together, not just embed. In testing this
 * beat every embed-only-parallel variant by ~4x on total pipeline time,
 * a direct illustration of Amdahl's law: speeding up only part of the
 * pipeline caps overall gains at that part's share of the total time.
 *
 * read/chunk/embed times are no longer separable per stage (they're
 * interleaved within each file's thread), so this reports one combined
 * "read+chunk+embed" time per trial instead of three separate lines.
 *
 * Usage:
 *   mvn compile exec:java -Dexec.mainClass=ChunkEmbedInsertPerFile -Dexec.args="corpus"
 */
public class ChunkEmbedInsertPerFile {
    private static final int TRIALS = 5;
    private static final Pattern GUTENBERG_HEADER =
            Pattern.compile("(?s)^.*?\\*\\*\\* START OF[^\\n]*\\*\\*\\*\\s*");
    private static final Pattern GUTENBERG_FOOTER =
            Pattern.compile("(?s)\\*\\*\\* END OF[^\\n]*\\*\\*\\*.*$");

    public static void main(String[] args) throws InterruptedException, IOException {
        if (args.length < 1) {
            System.err.println("usage: ChunkEmbedInsertPerFile <directory> [query]");
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

        @SuppressWarnings("unchecked")
        List<TextSegment>[] perFileSegments = new List[files.size()];
        @SuppressWarnings("unchecked")
        List<Embedding>[] perFileEmbeddings = new List[files.size()];

        long[] threadFinishNanos = new long[files.size()];

        for (int trial = 1; trial <= TRIALS; trial++) {
            long t0 = System.nanoTime();

            Thread[] threads = new Thread[files.size()];
            for (int i = 0; i < files.size(); i++) {
                int idx = i;
                threads[idx] = new Thread(() -> {
                    Document document = stripBoilerplate.transform(FileSystemDocumentLoader.loadDocument(files.get(idx)));
                    List<TextSegment> segments = splitter.split(document);
                    perFileSegments[idx] = segments;
                    perFileEmbeddings[idx] = model.embedAll(segments).content();
                    threadFinishNanos[idx] = System.nanoTime() - t0; // time from trial start until this file is done
                });
                threads[idx].start();
            }
            for (Thread thread : threads) {
                thread.join();
            }

            long totalNanos = System.nanoTime() - t0;
            int segmentCount = 0;
            for (List<TextSegment> segments : perFileSegments) segmentCount += segments.size();
            System.out.printf("trial %d/%d  %2d files  %6d segments  %8.3f s  (%d threads, read+chunk+embed)%n",
                    trial, TRIALS, files.size(), segmentCount, totalNanos / 1e9, files.size());

            if (trial == TRIALS) {
                System.out.println("  per-file finish times (steady-state trial, skew check):");
                Integer[] order = new Integer[files.size()];
                for (int i = 0; i < order.length; i++) order[i] = i;
                java.util.Arrays.sort(order, (a, b) -> Long.compare(threadFinishNanos[b], threadFinishNanos[a]));
                for (int i : order) {
                    System.out.printf("    %-28s %6d segments  finished at %6.3f s%n",
                            files.get(i).getFileName(), perFileSegments[i].size(), threadFinishNanos[i] / 1e9);
                }
            }
        }

        List<TextSegment> allSegments = new ArrayList<>();
        List<Embedding> allEmbeddings = new ArrayList<>();
        for (int i = 0; i < files.size(); i++) {
            allSegments.addAll(perFileSegments[i]);
            allEmbeddings.addAll(perFileEmbeddings[i]);
        }

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
}
