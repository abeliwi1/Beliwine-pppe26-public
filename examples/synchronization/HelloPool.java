/*
 * HelloPool.java -- tasks on a thread pool
 *
 * A fixed thread pool starts nthreads threads once and keeps them. You
 * submit tasks; each idle thread takes the next task and runs it. Here
 * there are twice as many tasks as threads, so each thread runs several:
 * the task is the work, the thread is what runs it.
 *
 * The pool is AutoCloseable (Java 19+): close(), at the end of the try
 * block, waits for every submitted task to finish, like join().
 *
 * Usage: java HelloPool.java [threads]     (default 4)
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class HelloPool {
    public static void main(String[] args) {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        try (ExecutorService pool = Executors.newFixedThreadPool(nthreads)) {
            for (int i = 0; i < 2 * nthreads; i++) {
                int id = i;
                pool.submit(() -> System.out.println(
                    "task " + id + " on " + Thread.currentThread().getName()));
            }
        }                                   // close() waits for every task

        System.out.println("all tasks done");
    }
}
