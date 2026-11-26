import java.util.ArrayList;

public class ListInt {
    public static void main(String[] args) {
        ArrayList<Integer> list = new ArrayList<>();
        int n = 1000000;
        for (int i = 0; i < n; i++) {
            list.add(i);
        }
        long sum = 0L;
        for (int i = 0; i < n; i++) {
            sum += list.get(i);
        }
        System.out.println("sum=" + sum);
    }
}
