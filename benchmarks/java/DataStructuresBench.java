import java.util.ArrayList;
import java.util.HashMap;
import java.util.PriorityQueue;
import java.util.TreeMap;

public class DataStructuresBench {
    public static void main(String[] args) {
        int n = 1_000_000;

        // 1. ArrayList<Integer>: 1M insert, 1M read, 1M remove(last)
        long t0 = System.nanoTime();
        ArrayList<Integer> list = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            list.add(i);
        }
        long sum = 0L;
        for (int i = 0; i < n; i++) {
            sum += list.get(i);
        }
        while (!list.isEmpty()) {
            list.remove(list.size() - 1);
        }
        long t1 = System.nanoTime();
        System.out.println("ArrayList (1M ins/read/rem): " + ((t1 - t0) / 1_000_000) + " ms, sum=" + sum);

        // 2. HashMap<Integer, Integer>: 1M insert, 1M read, 1M remove
        long t2 = System.nanoTime();
        HashMap<Integer, Integer> hmap = new HashMap<>();
        for (int i = 0; i < n; i++) {
            hmap.put(i, i + 1);
        }
        long hsum = 0L;
        for (int i = 0; i < n; i++) {
            hsum += hmap.get(i);
        }
        for (int i = 0; i < n; i++) {
            hmap.remove(i);
        }
        long t3 = System.nanoTime();
        System.out.println("HashMap (1M ins/read/rem): " + ((t3 - t2) / 1_000_000) + " ms, sum=" + hsum);

        // 3. TreeMap<Integer, Integer>: 1M insert, 1M read, 1M remove
        long t4 = System.nanoTime();
        TreeMap<Integer, Integer> tmap = new TreeMap<>();
        for (int i = 0; i < n; i++) {
            int k = (int) ((i * 2654435761L) & 1048575L);
            tmap.put(k, i);
        }
        long tsum = 0L;
        for (int i = 0; i < n; i++) {
            int k = (int) ((i * 2654435761L) & 1048575L);
            tsum += tmap.getOrDefault(k, 0);
        }
        for (int i = 0; i < n; i++) {
            int k = (int) ((i * 2654435761L) & 1048575L);
            if (tmap.containsKey(k)) {
                tmap.remove(k);
            }
        }
        long t5 = System.nanoTime();
        System.out.println("TreeMap (1M ins/read/rem): " + ((t5 - t4) / 1_000_000) + " ms, sum=" + tsum);

        // 4. PriorityQueue<Integer>: 1M offer, 1M peek, 1M poll
        long t6 = System.nanoTime();
        PriorityQueue<Integer> pq = new PriorityQueue<>();
        for (int i = 0; i < n; i++) {
            pq.offer((int) (((i * 1103515245L + 12345L) & 1048575L)));
        }
        long psum = 0L;
        while (!pq.isEmpty()) {
            psum += pq.peek();
            pq.poll();
        }
        long t7 = System.nanoTime();
        System.out.println("PriorityQueue (1M ins/peek/rem): " + ((t7 - t6) / 1_000_000) + " ms, sum=" + psum);
    }
}
