public class GcStress {
    static class Node {
        public int val;
        public Node next;

        public Node(int val, Node next) {
            this.val = val;
            this.next = next;
        }
    }

    public static void main(String[] args) {
        Node head = null;
        for (int i = 0; i < 1000; i++) {
            head = new Node(i, head);
        }
        int acc = 0;
        for (int i = 0; i < 500000; i++) {
            Node tmp = new Node(i, null);
            acc += tmp.val & 1;
        }
        System.out.println("gc_done acc=" + acc + " head=" + head.val);
    }
}
