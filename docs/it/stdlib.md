# Riferimento Libreria Standard Moka

> 🌐 **Lingua**: [English](../en/stdlib.md) | **Italiano**

## Classi Core Caricate Automaticamente

Queste classi sono incluse automaticamente in ogni compilazione (salvo l'uso di `--no-stdlib`):

### `String` (`std/String.moka`)
- `int length()`
- `char charAt(int i)` / `byte byteAt(int i)`
- `byte[] bytes()`
- `boolean equals(String other)`
- `String concat(String other)` — restituisce una nuova `String` (immutabile)
- `String append(String other)` — modifica `this` sul posto e restituisce `this`
- `String substring(int start, int end)`
- `int toInt()`, `long toLong()`, `double toDouble()`, `float toFloat()` — lanciano `Exception` se il formato non è valido
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

### Concorrenza (`std/Runnable.moka`, `std/Thread.moka`, `std/Mutex.moka`)
- `interface Runnable`: `void run()`
- `class Thread implements Runnable`: `Thread()`, `Thread(Runnable target)`, `void start()`, `void join()`, `static void sleep(int ms)`
- `class Mutex`: `Mutex()`, `void lock()`, `void unlock()`

---

## Moduli su Richiesta (`import`)

### `Math` (`import std/Math;` oppure `import Math;`)
`public static class Math`:
- Costanti: `Math.PI`, `Math.E`
- `abs(int)`, `abs(long)`, `abs(double)`
- `min(int, int)`, `max(int, int)`, `min(double, double)`, `max(double, double)`
- `sqrt(double)`, `pow(double, double)`, `sin(double)`, `cos(double)`, `tan(double)`
- `log(double)`, `exp(double)`, `floor(double)`, `ceil(double)`, `round(double)`

### Strutture Dati (`import std/datastructures/<Nome>;`)
- **`List<T>`** (interfaccia): `size()`, `isEmpty()`, `add(T)`, `get(int)`, `set(int, T)`, `removeAt(int)`, `clear()`
- **`ArrayList<T> implements List<T>`**: array dinamico basato su `T[]`, lancia `Exception` su indice fuori limite e azzera gli slot liberati con `default(T)`.
- **`Stack<T>`**: `push(T)`, `pop()`, `peek()`, `size()`, `isEmpty()`.
- **`Queue<T>`**: coda a buffer circolare con `enqueue(T)`, `dequeue()`, `peek()`, `size()`, `isEmpty()`.
- **`Map<K, V>`**: mappa chiave-valore (confronto `==` sulle chiavi) con `put(K, V)`, `get(K)`, `containsKey(K)`, `remove(K)`, `size()`, `isEmpty()`.
- **`StringMap<V>`**: mappa chiave-valore per chiavi `String` (confronto tramite `String.equals`).
