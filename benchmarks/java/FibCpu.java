public class FibCpu {
    public static int fib(int n) {
        if (n <= 1) {
            return n;
        }
        return fib(n - 1) + fib(n - 2);
    }

    public static void main(String[] args) {
        int res = fib(42);
        System.out.println("fib(42)=" + res);
    }
}
