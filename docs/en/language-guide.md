# Moka Language Guide

> 🌐 **Language**: **English** | [Italiano](../it/language-guide.md)

## 1. Program Entry Point

Every Moka program defines a `Main` class with a `public static int main()` (or `void main()`) entry point:

```moka
class Main {
    public static int main() {
        Console.println("Hello, Moka!");
        return 0;
    }
}
```

Command-line arguments are accessed via `Io.args()` (returns `String[]`), `Io.argc()`, and `Io.argv(int index)`.

---

## 2. Types, Literals, and `default(T)`

| Moka Type | C11 Representation | Example Literals |
|---|---|---|
| `byte` | `uint8_t` | `(byte) 255` |
| `char` | `uint16_t` | `'A'`, `'\n'` |
| `int` | `int32_t` | `42`, `-10` |
| `long` | `int64_t` | `10000000000L` |
| `float` | `float` | `2.5f`, `10f` |
| `double` | `double` | `3.14`, `1.0d` |
| `boolean` | `bool` | `true`, `false` |
| `T[]` | `RtArray*` | `new int[10]`, `arr.length` |
| `Class` / `Interface` | `Cls*` / `{ void*, VTable* }` | `new Box<int>(1)`, `null` |

### Numeric Promotions & Casts
- Implicit widening: `byte`/`char` -> `int` -> `long` -> `float` -> `double`.
- Explicit narrowing casts: `(int) 3.9`, `(byte) n`, `(char) 65`, `(float) d`.

### `default(T)`
`default(T)` produces the zero/null value for any type `T` (including generic type parameters):
- `default(int)` -> `0`, `default(double)` -> `0.0`, `default(boolean)` -> `false`, `default(String)` -> `null`.

---

## 3. Operators & Control Flow

- **Arithmetic**: `+`, `-`, `*`, `/`, `%` (integral only)
- **Increment / Compound**: `++`, `--`, `+=`, `-=`, `*=`, `/=`, `%=`
- **Bitwise / Shifts**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Comparison & Logical**: `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`
- **Control flow**: `if / else`, `while`, `for (init; cond; step)`, `switch (expr) { case X: ... default: ... }`, `break`, `continue`, `return`

---

## 4. Strings

- `+` and `+=` concatenate `String` with any primitive, `null`, or object reference.
- `concat(String)` returns a **new** `String` (immutable, Java-style).
- `append(String)` mutates `this` in place and returns `this`.
- `toInt()`, `toLong()`, `toDouble()`, `toFloat()` parse strings and throw `Exception` on invalid input.

```moka
String s = "Hello";
s.append(" ").append("Moka");      // mutates s -> "Hello Moka"
String msg = s + " #" + 1;         // new String -> "Hello Moka #1"
int n = "42".toInt();              // 42
```

---

## 5. Object-Oriented Programming

- **Implicit `this`**: inside instance methods and constructors, fields and methods of the current class (and superclasses) can be accessed without `this.`. Use `this.field` when shadowed by a parameter.
- **Inheritance & Polymorphism**: single inheritance with `extends`, `super(...)`, `super.method(...)`, virtual dispatch via VTables.
- **Interfaces**: `interface` and `implements A, B` (fat pointers `{ obj, vtable }`).
- **`abstract`**: abstract classes and abstract methods (must be implemented by concrete subclasses).
- **`final`**:
  - `final class`: cannot be extended.
  - `final` method: cannot be overridden.
  - `final` field / local variable: cannot be reassigned after initialization (inline field initializers like `public static final double PI = 3.14;` are supported).
- **`static class`**: cannot be instantiated or extended; all members must be `static` (e.g., `public static class Math`).
- **Overloading**: methods and constructors can be overloaded by parameter types.
- **RTTI**: `obj instanceof Type` and checked downcasts `(SubClass) baseRef` / `(MyInterface) obj`.

---

## 6. Exceptions

```moka
class MyException extends Exception {
    public MyException(String msg) {
        super(msg);
    }
}

try {
    int n = "abc".toInt();
} catch (MyException me) {
    Console.println(me.getMessage());
} catch (Exception e) {
    Console.println("Error: " + e.getMessage());
} finally {
    Console.println("Cleanup");
}
```

`finally` blocks always execute on normal exit, `return`, or `throw`.

---

## 7. Generics (Monomorphization)

Moka compiles generics by **monomorphization**: each concrete instantiation (e.g. `Box<int>`, `Box<String>`) generates a specialized C11 type and functions, so primitive type arguments work directly with zero boxing overhead.

```moka
interface Container<T> {
    T get();
}

class Box<T> implements Container<T> {
    private T value;
    public Box(T value) { this.value = value; }
    public T get() { return value; }
}

class Util {
    // Simplified syntax
    public static T identity<T>(T x) { return x; }
    // Java-compatible syntax
    public static <T> T first(T a, T b) { return a; }
}

Box<int> b1 = new Box<int>(42);
Box<String> b2 = new Box<>("hi");   // diamond <>
Box<double> b3 = new Box(3.5);      // inferred from argument
int x = Util.identity(10);          // inferred
String s = Util.identity<String>("ok");
```

---

## 8. Concurrency

- Implement `Runnable` (`void run()`) or extend `Thread`.
- Start with `t.start()`, wait with `t.join()`, pause with `Thread.sleep(ms)`.
- Protect shared state using `synchronized (obj) { ... }`, `synchronized` instance methods, or `Mutex` (`lock()` / `unlock()`).

```moka
class Counter {
    public int value;
    public synchronized void inc() {
        value = value + 1;
    }
}
```

---

## 9. Imports

- Local files: `import "helper.moka";`
- Standard library modules: `import std/Math;` (or `import Math;`), `import std/datastructures/ArrayList;` (or `import datastructures/ArrayList;`).
