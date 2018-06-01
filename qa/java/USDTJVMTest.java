/*
 * PCP BCC PMDA USDT JVM test helper
 */

import java.util.concurrent.ThreadLocalRandom;

class TestUnit implements Runnable {
    private int lifetime;

    public TestUnit(int lifetime) {
        this.lifetime = lifetime * 100;
    }

    public void run() {
        int i = 0;
        while (lifetime-- > 0) {
            i++;
            try {
                Thread.sleep(10);
            } catch (Exception ex) { ex.printStackTrace(); }
        }
    }
}

public class USDTJVMTest {
    public static void main(String[] args) {
        int i = 0;
        while (true) {
            i++;
            try {
                Thread.sleep(100);
            } catch (Exception ex) { ex.printStackTrace(); }

            int lifetime = ThreadLocalRandom.current().nextInt(1, 21);
            Thread t = new Thread(new TestUnit(lifetime));
            t.start();
        }
    }
}
