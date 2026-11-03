/*
 * HelloThreads.java -- the mechanics of a Java thread
 *
 * A Thread runs a Runnable: the code the thread executes, written here
 * as a lambda. start() returns at once in the caller and runs the lambda
 * in a new thread; join() waits for that thread to finish.
 *
 * This is what a thread pool is made of. Programs use a pool (see
 * HelloPool.java) rather than creating threads one at a time.
 *
 * Usage: java HelloThreads.java [threads]     (default 4)
 */
public class HelloThreads {
    public static void main(String[] args) throws InterruptedException {
        int nthreads = args.length > 0 ? Integer.parseInt(args[0]) : 4;

        Thread[] threads = new Thread[nthreads];
        for (int i = 0; i < nthreads; i++) {
            int id = i;                     // each lambda captures its own copy
            threads[i] = new Thread(() -> System.out.println("thread " + id + " running"));
            threads[i].start();             // returns at once; the lambda runs in the new thread
        }
        for (Thread t : threads) t.join();  // wait for each thread to finish

        System.out.println("all threads done");
    }
}
