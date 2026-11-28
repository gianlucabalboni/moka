<p align="center">
  <img src="assets/moka-logo.png" alt="Logo Moka" width="128" />
</p>

# Moka

> 🌐 **Lingua**: [English](README.md) | **Italiano**

**Scrivi in Java. Distribuisci in C11.**

**Moka** è un linguaggio di programmazione staticamente tipizzato, con sintassi ispirata a Java, che compila ahead-of-time in **C11 standard** (`-std=c11 -O2 -pthread -lm`) **senza JVM, senza bytecode e senza dipendenze esterne**.

---

## Perché scegliere Moka al posto di Java?

| | Moka | Java (OpenJDK 21) |
|---|---|---|
| **Avvio a freddo (Cold Start)** | **`1.1 ms`** binario nativo | `31.4 ms` avvio JVM (`~28x` più lento) |
| **Generici** | **Monomorfizzati e unboxed** (`ArrayList<int>`, `HashMap<int, int>`, `PriorityQueue<int>`) | Type-erasure con boxing (`Integer` allocato su heap) |
| **Consumo di memoria** | **`13.51 MB`** (`ListInt`), **`118.40 MB`** (`DataStructuresBench`) | `76.12 MB` (`ListInt`), `290.97 MB` (`DataStructuresBench`) |
| **Distribuzione** | Un singolo eseguibile nativo standalone | Richiede JRE / JVM installata |
| **Interop con C** | Integrazione diretta `native` in C11 | Boilerplate JNI |

- **Ergonomia familiare da Java**: classi, ereditarietà singola (`extends`), interfacce con ereditarietà (`interface A extends B, C`), `abstract`, `final`, `try / catch / finally / throw`, `synchronized`, `Thread` / `Mutex`, `for (T x : col)` (`for-each`), diamond `<>` con inferenza, e libreria `std/datastructures/*` ispirata a `java.util`.
- **Generici sui primitivi a zero overhead**: `ArrayList<int>`, `TreeMap<int, int>` e `PriorityQueue<int>` memorizzano valori `int32_t` contigui con **zero boxing**.
- **Self-Hosting verificato**: `stage1` compila se stesso generando C11 identico byte-per-byte (`stage1_gen1.c == stage1_gen2.c`).

---

## Esempi

### 1. Generici unboxed, `HashMap`, `ArrayList`, ordinamento e `for-each`

```moka
import std/Comparator;
import std/datastructures/ArrayList;
import std/datastructures/HashMap;
import std/datastructures/Hasher;
import std/datastructures/MapEntry;

class Main {
    public static int main() {
        // ArrayList<int> senza boxing + merge sort stabile
        ArrayList<int> nums = new ArrayList<>();
        nums.add(42);
        nums.add(7);
        nums.add(19);
        nums.sort(new IntComparator());

        for (int n : nums) {
            Console.println("num = " + n);
        }

        // HashMap<String, int> open-addressing (valori int non boxati)
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

### 2. Eccezioni, Interfacce e `PriorityQueue`

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
            pq.remove(); // lancia NoSuchElementException su coda vuota
        } catch (NoSuchElementException e) {
            Console.println("Eccezione catturata: " + e.getMessage());
        }
        return 0;
    }
}
```

---

## Benchmark (Moka vs OpenJDK 21)

Misurati con [`python3 benchmarks/run_benchmarks.py`](benchmarks/run_benchmarks.py) (mediana di 3 esecuzioni tramite `os.wait4`; dettagli completi in [benchmarks/README.it.md](benchmarks/README.it.md)):

| Benchmark | Descrizione | Tempo Moka | Tempo Java | Picco RSS Moka | Picco RSS Java |
|---|---|---:|---:|---:|---:|
| **Startup** | Hello World minimale (cold start) | **`1.0 ms`** | `31.9 ms` | **`13.91 MB`** | `42.14 MB` |
| **FibCpu** | `fib(42)` ricorsivo | **`485.5 ms`** | `1187.7 ms` | **`13.91 MB`** | `43.04 MB` |
| **ListInt** | `1.000.000` di `int` in `ArrayList` + somma | **`20.7 ms`** | `68.5 ms` | **`13.91 MB`** | `76.04 MB` |
| **GcStress** | `500.000` allocazioni temp + `1.000` nodi vivi | **`24.5 ms`** | `56.1 ms` | **`13.91 MB`** | `55.84 MB` |
| **DataStructuresBench** | 1M op ciascuna su `ArrayList`, `HashMap`, `TreeMap`, `PriorityQueue` | **`1904.1 ms`** | `2425.6 ms` | **`118.05 MB`** | `285.52 MB` |

In `DataStructuresBench` (1.000.000 operazioni insert/read/remove per struttura), Moka vince complessivamente (`1904.1 ms` vs `2425.6 ms`), con `PriorityQueue<int>` in **`348 ms`** vs **`759 ms`** (`~2.2x` più veloce) e `TreeMap<int, int>` in **`1107 ms`** vs **`1397 ms`**, riducendo il picco RSS di **`2.42x`**.

---

## Architettura

Il progetto segue una strategia di **bootstrapping (self-hosting)** in due stadi:

1. **[`stage0/`](stage0/README.md)** — Compilatore bootstrap minimale scritto in **C11** (`stage0/src/*.c`), con Arena Allocator e supporto al subset core del linguaggio.
2. **[`stage1/`](stage1/README.md)** — Compilatore **self-hosting** completo scritto in **Moka** (`stage1/src/*.moka`), dotato di:
   - **Tipi primitivi**: `int`, `long`, `float`, `double`, `byte`, `char`, `boolean` + `default(T)`
   - **OOP e Interfacce**: ereditarietà singola (`extends`), `super`, VTable, `interface` con ereditarietà (`interface A extends B, C`), `abstract`, `final`, `static class`, overloading, `instanceof` e downcast controllati
   - **Controllo di flusso**: `for`, `for (T x : col)` (`for-each`), `while`, `switch`, `try / catch / finally / throw`
   - **Generici tramite Monomorfizzazione**: classi, interfacce e metodi generici per primitivi e oggetti
   - **Concorrenza e GC**: `Thread`, `Runnable`, `Mutex`, `synchronized`, Garbage Collector Mark-and-Sweep Stop-the-World
   - **Libreria Standard**: `String` (`Comparable<String>`, `hashCode` con cache), `Comparable<T>`, `Comparator<T>`, `Console`, `Io`, `ByteBuffer`, `Math`, gerarchia eccezioni (`std/Exceptions`) e `std/datastructures/*` (`Iterator`, `Iterable`, `Collection`, `List`, `Queue`, `Deque`, `Set`, `Map`, `ArrayList`, `ArrayDeque`, `LinkedList`, `Stack`, `PriorityQueue`, `HashMap`, `HashSet`, `TreeMap`, `TreeSet`, `Collections`)

---

## Quick Start

```bash
# 1. Compila e testa Stage 0 (compilatore bootstrap in C11)
make -C stage0 all

# 2. Compila Stage 1 tramite Stage 0, esegui tutte le 26 suite di test e verifica il bootstrap
make -C stage1 all
```

### Installazione Globale (`mokac` CLI)

```bash
# Installa mokac, std/ e runtime/ in ~/.local/ (aggiunge `mokac` in ~/.local/bin)
make -C stage1 install

# Compila in un eseguibile ed eseguilo
mokac hello.moka -o hello
./hello

# Oppure compila ed esegui al volo da qualsiasi directory
mokac -r hello.moka

# Disinstalla globalmente in un colpo solo
make -C stage1 uninstall
```

---

## Documentazione

- **Italiano**:
  - [Indice Documentazione](docs/it/README.md)
  - [Guida al Linguaggio](docs/it/language-guide.md)
  - [Libreria Standard](docs/it/stdlib.md)
  - [CLI, Installazione e Runtime](docs/it/cli-and-runtime.md)
  - [Architettura Interna (Stage 0 & Stage 1)](docs/it/internals.md)
- **English**:
  - [Documentation Index](docs/en/README.md)
  - [Language Guide](docs/en/language-guide.md)
  - [Standard Library Reference](docs/en/stdlib.md)
  - [CLI, Installation & Runtime](docs/en/cli-and-runtime.md)
  - [Compiler Internals (Stage 0 & Stage 1)](docs/en/internals.md)

---

## Struttura del Repository

```text
moka/
├── README.md              # Panoramica del progetto (English)
├── README.it.md           # Panoramica del progetto (Italiano)
├── benchmarks/            # Suite di benchmark Moka vs Java
├── docs/                  # Documentazione linguaggio, stdlib e architettura (en/ & it/)
├── examples/              # Programmi di esempio Moka
├── stage0/                # Compilatore bootstrap minimale in C11
└── stage1/                # Compilatore self-hosting, runtime, stdlib e test
    ├── Makefile
    ├── runtime/           # Runtime C11 (GC STW, eccezioni, pthread, monitor)
    ├── src/               # Sorgenti compilatore Stage 1
    ├── std/               # Libreria standard Stage 1 completa
    ├── std_boot/          # Stdlib compatibile Stage 0 per compilare stage1/src
    └── tests/             # Suite di test end-to-end
```
