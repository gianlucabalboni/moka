# Moka vs Java Benchmarks

> 🌐 **Language**: **English** | [Italiano](README.it.md)

This directory contains a comparative benchmark suite between **Moka** (`mokac` compiling to C11 with `-O2`) and **Java** (`javac` + `java` on OpenJDK `21.0.13.1`).

## How to Run

```bash
python3 benchmarks/run_benchmarks.py
```

The runner compiles each `.moka` and `.java` program, runs 3 iterations per case via `os.wait4`, and reports the **median** wall-clock time and peak Resident Set Size (RSS).

---

## Results

| Benchmark | Description | Moka Time | Java Time | Moka Peak RSS | Java Peak RSS | Source |
|---|---|---:|---:|---:|---:|---|
| **Startup** | Minimal Hello World (cold start) | `1.4 ms` | `33.4 ms` | `13.79 MB` | `41.97 MB` | [Startup.moka](moka/Startup.moka) / [Startup.java](java/Startup.java) |
| **FibCpu** | Recursive `fib(42)` | `502.1 ms` | `1195.0 ms` | `13.79 MB` | `43.21 MB` | [FibCpu.moka](moka/FibCpu.moka) / [FibCpu.java](java/FibCpu.java) |
| **ListInt** | `1,000,000` ints in `ArrayList` + sum | `19.4 ms` | `72.9 ms` | `13.79 MB` | `76.02 MB` | [ListInt.moka](moka/ListInt.moka) / [ListInt.java](java/ListInt.java) |
| **GcStress** | `500,000` temporary `Node` allocations + `1,000` live nodes | `24.6 ms` | `60.1 ms` | `13.79 MB` | `55.71 MB` | [GcStress.moka](moka/GcStress.moka) / [GcStress.java](java/GcStress.java) |

---

## Notes

- **Cold Start (`Startup`)**: Moka compiles to a standalone native binary (`~24x` faster startup than JVM initialization).
- **CPU Throughput (`FibCpu`)**: With `-O2`, Moka completes `fib(42)` in `502.1 ms` vs `1195.0 ms` on OpenJDK 21 (`~2.4x` faster).
- **Memory & Monomorphized Generics (`ListInt`)**: Moka's `ArrayList<int>` stores unboxed `int32_t` elements contiguously (`13.79 MB` peak RSS, `19.4 ms`), whereas Java's `ArrayList<Integer>` boxes every integer on the heap (`76.02 MB` peak RSS, `72.9 ms`).
- **Garbage Collection (`GcStress`)**: For short-lived workloads (500k allocations), Moka finishes in `24.6 ms` vs `60.1 ms` for Java (which includes JVM startup/JIT overhead; on long-running multi-threaded allocation-heavy workloads, Java's generational GC and TLABs scale better than Moka's global Stop-the-World Mark-and-Sweep).
