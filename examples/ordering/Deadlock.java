/*
 * Deadlock.java -- two locks, taken in opposite orders
 *
 * A transfer between two accounts must update both at once, so it locks
 * both. Thread A transfers from x to y: it locks x, then y. Thread B
 * transfers from y to x: it locks y, then x. If A holds x and B holds y,
 * each waits forever for the lock the other holds: a deadlock.
 *
 * Each lock is correct on its own. The bug is in the order the two are
 * taken, and it appears only when the two threads' timing lines up, so a
 * program can pass its tests for months and then hang.
 *
 * The fix is to agree on one order. With "ordered", every transfer locks
 * the account with the smaller id first, whichever way the money goes, so
 * no thread can hold the second lock while waiting for the first.
 *
 * Usage: java Deadlock.java [ordered]
 */
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

public class Deadlock {
    static class Account {
        final int id;
        long balance = 1_000_000;
        Account(int id) { this.id = id; }
    }

    static boolean ordered = false;

    static void transfer(Account from, Account to, long amount) {
        Account first = from, second = to;
        if (ordered && to.id < from.id) { first = to; second = from; }
        synchronized (first) {
            synchronized (second) {
                from.balance -= amount;
                to.balance += amount;
            }
        }
    }

    static final int TRANSFERS = 1_000_000;

    public static void main(String[] args) throws Exception {
        ordered = args.length > 0 && args[0].equals("ordered");
        Account x = new Account(1), y = new Account(2);

        ExecutorService pool = Executors.newFixedThreadPool(2);
        pool.submit(() -> { for (int i = 0; i < TRANSFERS; i++) transfer(x, y, 1); });   // A: x to y
        pool.submit(() -> { for (int i = 0; i < TRANSFERS; i++) transfer(y, x, 1); });   // B: y to x
        pool.shutdown();
        boolean finished = pool.awaitTermination(10, TimeUnit.SECONDS);

        System.out.printf("Deadlock  %s: %s%n", ordered ? "ordered locks" : "opposite orders",
                          finished ? String.format("finished, total %,d (%s)", x.balance + y.balance,
                                                   x.balance + y.balance == 2_000_000 ? "correct" : "WRONG")
                                   : "STUCK: both threads still waiting after 10 s");
        System.exit(0);                         // stops the deadlocked threads
    }
}
