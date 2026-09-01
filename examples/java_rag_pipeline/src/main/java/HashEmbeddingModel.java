import dev.langchain4j.data.embedding.Embedding;
import dev.langchain4j.data.segment.TextSegment;
import dev.langchain4j.model.embedding.EmbeddingModel;
import dev.langchain4j.model.output.Response;

import java.util.List;

import static java.util.stream.Collectors.toList;

/*
 * HashEmbeddingModel.java -- bag-of-words embedding via the hashing trick,
 * pure Java, no native code.
 *
 * Every word is hashed into one of DIMENSIONS buckets and counted; the
 * resulting count vector is L2-normalized. This is a real, well-known
 * technique (same idea as scikit-learn's HashingVectorizer) -- cruder
 * than a transformer embedding, but two chunks that share vocabulary
 * still score higher on cosine similarity, so search results stay
 * meaningful.
 *
 * The point of swapping this in for AllMiniLmL6V2EmbeddingModel is that
 * it has zero hidden parallelism: no ONNX Runtime thread pool, no Rust
 * tokenizer with its own rayon pool. Whatever a threaded version of this
 * pipeline does or doesn't speed up, the answer lives entirely in our
 * own Java code, not in a native library's internal scheduling.
 *
 * Only embedAll needs to be overridden -- EmbeddingModel's other methods
 * (embed(String), embed(TextSegment), ...) all funnel into it by default.
 */
public class HashEmbeddingModel implements EmbeddingModel {
    private static final int DIMENSIONS = 384;

    @Override
    public Response<List<Embedding>> embedAll(List<TextSegment> textSegments) {
        List<Embedding> embeddings =
                textSegments.stream().map(segment -> Embedding.from(hash(segment.text()))).collect(toList());
        return Response.from(embeddings);
    }

    private static float[] hash(String text) {
        float[] vector = new float[DIMENSIONS];
        for (String word : text.toLowerCase().split("\\W+")) {
            if (word.isEmpty()) continue;
            vector[Math.floorMod(word.hashCode(), DIMENSIONS)] += 1f;
        }

        double sumOfSquares = 0;
        for (float v : vector) sumOfSquares += v * v;
        if (sumOfSquares > 0) {
            float norm = (float) Math.sqrt(sumOfSquares);
            for (int i = 0; i < vector.length; i++) vector[i] /= norm;
        }
        return vector;
    }
}
