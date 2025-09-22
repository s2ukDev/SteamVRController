package org.s2uk.vrcontroller;

import java.util.concurrent.BlockingDeque;
import java.util.concurrent.LinkedBlockingDeque;

public class ServerMessages {
    private final BlockingDeque<String> queue = new LinkedBlockingDeque<>();

    public void enqueue(String message) {
        if (message != null) {
            queue.addLast(message);
        }
    }
    public String readLast() {
        return queue.pollFirst();
    }
    public int size() {
        return queue.size();
    }
    public void clear() {
        queue.clear();
    }
}
