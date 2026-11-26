# Moka Standard Library Reference

> 🌐 **Language**: **English** | [Italiano](../it/stdlib.md)

## Auto-Loaded Core Classes

These classes are automatically included in every compilation (unless `--no-stdlib` is passed):

### `String` (`std/String.moka`)
- `int length()`
- `char charAt(int i)` / `byte byteAt(int i)`
- `byte[] bytes()`
- `boolean equals(String other)`
- `String concat(String other)` — returns a new `String` (immutable)
- `String append(String other)` — mutates `this` in place and returns `this`
- `String substring(int start, int end)`
- `int toInt()`, `long toLong()`, `double toDouble()`, `float toFloat()` — throw `Exception` on invalid input
- `static String fromInt(int n)`, `fromLong(long n)`, `fromDouble(double d)`, `fromFloat(float f)`, `fromBoolean(boolean b)`, `fromByte(byte b)`, `fromChar(char c)`

### `Console` (`std/Console.moka`)
- `static void print(String s)`, `static void println(String s)`
- `static void eprint(String s)`, `static void eprintln(String s)`
- `static void printInt(int n)`
- `static String readLine()`, `static int readInt()`

### `Io` (`std/Io.moka`)
- `static String[] args()`, `static int argc()`, `static String argv(int index)`
- `static byte[] readFileBytes(String path)`
- `static boolean writeFileBytes(String path, byte[] data)`
- `static boolean removeFile(String path)`
- `static int exec(String cmd)`
- `static void exit(int code)`

### `ByteBuffer` (`std/ByteBuffer.moka`)
- `ByteBuffer(int initialCap)`
- `int length()`
- `void appendByte(byte b)`, `void appendBytes(byte[] src)`, `void appendString(String s)`, `void appendInt(int n)`
- `byte[] toBytes()`, `String toString()`

### `Exception` (`std/Exception.moka`)
- `Exception(String message)`
- `String getMessage()`

### Concurrency (`std/Runnable.moka`, `std/Thread.moka`, `std/Mutex.moka`)
- `interface Runnable`: `void run()`
- `class Thread implements Runnable`: `Thread()`, `Thread(Runnable target)`, `void start()`, `void join()`, `static void sleep(int ms)`
- `class Mutex`: `Mutex()`, `void lock()`, `void unlock()`

---

## On-Demand Modules (`import`)

### `Math` (`import std/Math;` or `import Math;`)
`public static class Math`:
- Constants: `Math.PI`, `Math.E`
- `abs(int)`, `abs(long)`, `abs(double)`
- `min(int, int)`, `max(int, int)`, `min(double, double)`, `max(double, double)`
- `sqrt(double)`, `pow(double, double)`, `sin(double)`, `cos(double)`, `tan(double)`
- `log(double)`, `exp(double)`, `floor(double)`, `ceil(double)`, `round(double)`

### Data Structures (`import std/datastructures/<Name>;`)
- **`List<T>`** (interface): `size()`, `isEmpty()`, `add(T)`, `get(int)`, `set(int, T)`, `removeAt(int)`, `clear()`
- **`ArrayList<T> implements List<T>`**: dynamic array backed by `T[]`, throws `Exception` on out-of-bounds, clears freed slots with `default(T)`.
- **`Stack<T>`**: `push(T)`, `pop()`, `peek()`, `size()`, `isEmpty()`.
- **`Queue<T>`**: circular-buffer queue with `enqueue(T)`, `dequeue()`, `peek()`, `size()`, `isEmpty()`.
- **`Map<K, V>`**: key-value map (`==` key comparison) with `put(K, V)`, `get(K)`, `containsKey(K)`, `remove(K)`, `size()`, `isEmpty()`.
- **`StringMap<V>`**: key-value map using `String.equals(...)` for `String` keys.
