<p align="center">
  <img src="assets/moka-logo.png" alt="Moka Logo" width="128" />
</p>

# Moka

> 🌐 **Language**: **English** | [Italiano](README.it.md)

**Write Java. Ship C11.**

**Moka** is a statically typed, Java-syntax programming language that compiles ahead-of-time to **standard C11** (`-std=c11 -O2 -pthread -lm`) with **zero JVM, zero bytecode, and zero external dependencies**.

---

## Why Moka instead of Java?

| | Moka | Java (OpenJDK 21) |
|---|---|---|
| **Cold Start** | **`1.1 ms`** native binary | `31.4 ms` JVM startup (`~28x` slower) |
| **Generics** | **Monomorphized & unboxed** (`ArrayList<int>`, `HashMap<int, int>`, `PriorityQueue<int>`) | Type-erased & boxed (`Integer` heap allocation per element) |
| **Memory Footprint** | **`13.51 MB`** (`ListInt`), **`118.40 MB`** (`DataStructuresBench`) | `76.12 MB` (`ListInt`), `290.97 MB` (`DataStructuresBench`) |
| **Deployment** | Single standalone native executable | Requires JRE / JVM runtime |
| **C Interop** | Direct `native` C11 integration | JNI boilerplate |

- **Familiar Java Ergonomics**: classes, single inheritance (`extends`), interfaces with inheritance (`interface A extends B, C`), `abstract`, `final`, `try / catch / finally / throw`, `synchronized`, `Thread` / `Mutex`, `for (T x : col)` (`for-each`), diamond `<>` inference, and a `java.util`-inspired `std/datastructures/*` library.
- **Zero-Overhead Primitive Generics**: `ArrayList<int>`, `TreeMap<int, int>`, and `PriorityQueue<int>` store `int32_t` values contiguously with **zero boxing**.
- **Self-Hosting & Verified**: `stage1` compiles itself into byte-for-byte identical C11 (`stage1_gen1.c == stage1_gen2.c`).

---

## Examples

### 1. Unboxed Generics, `HashMap`, `ArrayList`, Sorting & `for-each`

```moka
import std/Comparator;
import std/datastructures/ArrayList;
import std/datastructures/HashMap;
import std/datastructures/Hasher;
import std/datastructures/MapEntry;

class Main {
    public static int main() {
        // Unboxed ArrayList<int> + stable merge sort
        ArrayList<int> nums = new ArrayList<>();
        nums.add(42);
        nums.add(7);
        nums.add(19);
        nums.sort(new IntComparator());

        for (int n : nums) {
            Console.println("num = " + n);
        }

        // Open-addressed HashMap<String, int> (unboxed int values)
        HashMap<String, int> scores = new HashMap<>(new StringHasher());
        scores.put("Alice", 95);
        scores.put("Bob", 88);

        for (MapEntry<String, int> entry : scores.entrySet()) {
            Console.println(entry.getKey() + " -> " + entry.getValue());
        }
        return 0;
    }
}
```

### 2. Exceptions, Interfaces & `PriorityQueue`

```moka
import std/Comparable;
import std/Comparator;
import std/datastructures/PriorityQueue;

class Job implements Comparable<Job> {
    public int priority;
    public String name;

    public Job(int priority, String name) {
        this.priority = priority;
        this.name = name;
    }

    public int compareTo(Job other) {
        if (this.priority < other.priority) return -1;
        if (this.priority > other.priority) return 1;
        return 0;
    }
}

class Main {
    public static int main() {
        PriorityQueue<Job> pq = new PriorityQueue<>(new NaturalOrder<Job>());
        pq.offer(new Job(3, "low"));
        pq.offer(new Job(1, "urgent"));
        pq.offer(new Job(2, "normal"));

        while (!pq.isEmpty()) {
            Job j = pq.poll();
            Console.println(j.name + " (" + j.priority + ")");
        }

        try {
            pq.remove(); // throws NoSuchElementException on empty queue
        } catch (NoSuchElementException e) {
            Console.println("Caught: " + e.getMessage());
        }
        return 0;
    }
}
```

---

## Benchmarks (Moka vs OpenJDK 21)

Measured with [`python3 benchmarks/run_benchmarks.py`](benchmarks/run_benchmarks.py) (median of 3 runs via `os.wait4`; see [benchmarks/README.md](benchmarks/README.md) for full details):

| Benchmark | Description | Moka Time | Java Time | Moka Peak RSS | Java Peak RSS |
|---|---|---:|---:|---:|---:|
| **Startup** | Minimal Hello World (cold start) | **`1.0 ms`** | `31.9 ms` | **`13.91 MB`** | `42.14 MB` |
| **FibCpu** | Recursive `fib(42)` | **`485.5 ms`** | `1187.7 ms` | **`13.91 MB`** | `43.04 MB` |
| **ListInt** | `1,000,000` ints in `ArrayList` + sum | **`20.7 ms`** | `68.5 ms` | **`13.91 MB`** | `76.04 MB` |
| **GcStress** | `500,000` temp allocations + `1,000` live | **`24.5 ms`** | `56.1 ms` | **`13.91 MB`** | `55.84 MB` |
| **DataStructuresBench** | 1M ops each on `ArrayList`, `HashMap`, `TreeMap`, `PriorityQueue` | **`1904.1 ms`** | `2425.6 ms` | **`118.05 MB`** | `285.52 MB` |

In `DataStructuresBench` (1,000,000 insert/read/remove ops per structure), Moka wins overall (`1904.1 ms` vs `2425.6 ms`), with unboxed `PriorityQueue<int>` running in **`348 ms`** vs **`759 ms`** (`~2.2x` faster) and `TreeMap<int, int>` in **`1107 ms`** vs **`1397 ms`**, while cutting peak RSS by **`2.42x`**.

---

## Architecture

Moka uses a two-stage **self-hosting bootstrap** pipeline:

1. **[`stage0/`](stage0/README.md)** — Minimal bootstrap compiler written in **C11** (`stage0/src/*.c`), using an Arena Allocator and compiling the core language subset.
2. **[`stage1/`](stage1/README.md)** — Full **self-hosting** compiler written in **Moka** (`stage1/src/*.moka`), featuring:
   - **Primitives**: `int`, `long`, `float`, `double`, `byte`, `char`, `boolean` + `default(T)`
   - **OOP & Interfaces**: single inheritance (`extends`), `super`, VTable polymorphism, `interface` inheritance (`interface A extends B, C`), `abstract`, `final`, `static class`, overloading, `instanceof` & checked casts
   - **Control Flow**: `for`, `for (T x : col)` (`for-each`), `while`, `switch`, `try / catch / finally / throw`
   - **Generics via Monomorphization**: classes, interfaces, and methods with primitive and reference arguments
   - **Concurrency & GC**: `Thread`, `Runnable`, `Mutex`, `synchronized`, cooperative Stop-the-World Mark-and-Sweep GC
   - **Standard Library**: `String` (`Comparable<String>`, cached `hashCode`), `Comparable<T>`, `Comparator<T>`, `Console`, `Io`, `ByteBuffer`, `Math`, exception hierarchy (`std/Exceptions`), and `std/datastructures/*` (`Iterator`, `Iterable`, `Collection`, `List`, `Queue`, `Deque`, `Set`, `Map`, `ArrayList`, `ArrayDeque`, `LinkedList`, `Stack`, `PriorityQueue`, `HashMap`, `HashSet`, `TreeMap`, `TreeSet`, `Collections`)

---

## Quick Start

```bash
# 1. Build and test Stage 0 (C11 bootstrap compiler)
make -C stage0 all

# 2. Build Stage 1 via Stage 0, run all 26 test suites, and verify bootstrap fixed-point
make -C stage1 all
```

### Global Installation (`mokac` CLI)

```bash
# Install mokac, std/, and runtime/ to ~/.local/ (adds `mokac` to ~/.local/bin)
make -C stage1 install

# Compile to an executable and run it
mokac hello.moka -o hello
./hello

# Or compile and run in one step from any directory
mokac -r hello.moka

# Uninstall globally at any time
make -C stage1 uninstall
```

---

## Documentation

- **English**:
  - [Documentation Index](docs/en/README.md)
  - [Language Guide](docs/en/language-guide.md)
  - [Standard Library Reference](docs/en/stdlib.md)
  - [CLI, Installation & Runtime](docs/en/cli-and-runtime.md)
  - [Compiler Internals (Stage 0 & Stage 1)](docs/en/internals.md)
- **Italiano**:
  - [Indice Documentazione](docs/it/README.md)
  - [Guida al Linguaggio](docs/it/language-guide.md)
  - [Libreria Standard](docs/it/stdlib.md)
  - [CLI, Installazione e Runtime](docs/it/cli-and-runtime.md)
  - [Architettura Interna (Stage 0 & Stage 1)](docs/it/internals.md)

---

## Repository Layout

```text
moka/
├── README.md              # Project overview (English)
├── README.it.md           # Project overview (Italiano)
├── benchmarks/            # Moka vs Java benchmark suite
├── docs/                  # Language, stdlib & internals documentation (en/ & it/)
├── examples/              # Example Moka programs
├── stage0/                # Minimal C11 bootstrap compiler
└── stage1/                # Self-hosting Moka compiler, runtime, stdlib & tests
    ├── Makefile
    ├── runtime/           # C11 runtime (STW GC, exceptions, pthreads, monitors)
    ├── src/               # Stage 1 compiler sources (in Stage 0 Moka subset)
    ├── std/               # Full Stage 1 standard library
    ├── std_boot/          # Stage-0-compatible stdlib used to compile stage1/src
    └── tests/             # End-to-end test suite
```
