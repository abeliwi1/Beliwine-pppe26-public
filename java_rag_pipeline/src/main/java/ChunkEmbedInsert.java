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

import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.List;
import java.util.regex.Pattern;

/*
 * ChunkEmbedInsert.java -- chunk / embed / insert pipeline over a real corpus
 *
 * Real documents (the Project Gutenberg books in corpus/) are hundreds of
 * KB -- far past what one embedding call can meaningfully represent as a
 * single vector. So this:
 *
 *   1. loads each book as a Document (FileSystemDocumentLoader)
 *   2. strips the Project Gutenberg license header/footer (every book
 *      has identical boilerplate; left in, it would dominate every chunk)
 *   3. splits each Document into overlapping TextSegments (DocumentSplitters
 *      .recursive: paragraphs first, falling back to lines/sentences/words
 *      for anything too long to fit)
 *   4. embeds each segment (HashEmbeddingModel -- see that file) and
 *      inserts it into an InMemoryEmbeddingStore
 *
 * Each stage is timed separately (read / chunk / embed / update) instead
 * of going through EmbeddingStoreIngestor, which bundles all four into one
 * opaque ingest() call. This is the serial baseline: model.embedAll() here
 * is a single-threaded stream, comparable against ChunkEmbedInsertPerFile
 * .java's one-thread-per-file version, which parallelizes chunk and embed
 * together instead of embed alone.
 *
 * The embed stage runs TRIALS times in the same JVM invocation (same idea
 * as dot_product.c's TRIALS loop) so later trials benefit from whatever
 * JIT warm-up happened during earlier ones -- a single ~0.25s run finishes
 * before HotSpot's tiered compiler would ever kick in on the hot method.
 *
 * Usage:
 *   mvn compile exec:java -Dexec.args="corpus"
 *   mvn compile exec:java -Dexec.args="corpus 'a scientist creates a monster'"
 */
public class ChunkEmbedInsert {
    private static final int TRIALS = 5;
    private static final Pattern GUTENBERG_HEADER =
            Pattern.compile("(?s)^.*?\\*\\*\\* START OF[^\\n]*\\*\\*\\*\\s*");
    private static final Pattern GUTENBERG_FOOTER =
            Pattern.compile("(?s)\\*\\*\\* END OF[^\\n]*\\*\\*\\*.*$");

    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("usage: ChunkEmbedInsert <directory> [query]");
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

        long t0 = System.nanoTime();
        List<Document> documents = FileSystemDocumentLoader.loadDocuments(dir);
        long readNanos = System.nanoTime() - t0;

        long t1 = System.nanoTime();
        List<TextSegment> segments = splitter.splitAll(stripBoilerplate.transformAll(documents));
        long chunkNanos = System.nanoTime() - t1;

        System.out.printf("%-8s %6d documents  %8.3f s%n", "read", documents.size(), readNanos / 1e9);
        System.out.printf("%-8s %6d segments   %8.3f s%n", "chunk", segments.size(), chunkNanos / 1e9);

        List<Embedding> embeddings = null;
        for (int trial = 1; trial <= TRIALS; trial++) {
            long t2 = System.nanoTime();
            embeddings = model.embedAll(segments).content();
            long embedNanos = System.nanoTime() - t2;
            System.out.printf("embed    trial %d/%d  %6d embeddings %8.3f s%n",
                    trial, TRIALS, embeddings.size(), embedNanos / 1e9);
        }

        long t3 = System.nanoTime();
        store.addAll(embeddings, segments);
        long updateNanos = System.nanoTime() - t3;

        Path outFile = dir.resolveSibling(dir.getFileName() + "_embeddings_store.json");
        store.serializeToFile(outFile);

        System.out.printf("%-8s %6d entries    %8.3f s%n", "update", segments.size(), updateNanos / 1e9);
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
