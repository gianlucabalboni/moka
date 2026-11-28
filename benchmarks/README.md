# Moka vs Java Benchmarks

> 🌐 **Language**: **English** | [Italiano](README.it.md)

This directory contains a comparative benchmark suite between **Moka** (`mokac` compiling to C11 with `-O2`) and **Java** (`javac` + `java` on OpenJDK `21`).

## How to Run

```bash
python3 benchmarks/run_benchmarks.py
```

The runner compiles each `.moka` and `.java` program (preferring `stage1/build/mokac` if built, otherwise `mokac` from `PATH`), runs 3 iterations per case via `os.wait4`, and reports the **median** wall-clock time and peak Resident Set Size (RSS).

---

## Results

| Benchmark | Description | Moka Time | Java Time | Moka Peak RSS | Java Peak RSS | Source |
|---|---|---:|---:|---:|---:|---|
| **Startup** | Minimal Hello World (cold start) | `1.1 ms` | `31.4 ms` | `13.51 MB` | `42.12 MB` | [Startup.moka](moka/Startup.moka) / [Startup.java](java/Startup.java) |
| **FibCpu** | Recursive `fib(42)` | `487.0 ms` | `1183.2 ms` | `13.51 MB` | `42.87 MB` | [FibCpu.moka](moka/FibCpu.moka) / [FibCpu.java](java/FibCpu.java) |
| **ListInt** | `1,000,000` ints in `ArrayList` + sum | `20.6 ms` | `66.9 ms` | `13.51 MB` | `76.12 MB` | [ListInt.moka](moka/ListInt.moka) / [ListInt.java](java/ListInt.java) |
| **GcStress** | `500,000` temporary `Node` allocations + `1,000` live nodes | `23.5 ms` | `54.9 ms` | `13.51 MB` | `55.91 MB` | [GcStress.moka](moka/GcStress.moka) / [GcStress.java](java/GcStress.java) |
| **DataStructuresBench** | `1,000,000` ops (insert/read/remove) on `ArrayList`, `HashMap`, `TreeMap`, `PriorityQueue` | `1934.2 ms` | `1599.8 ms` | `118.40 MB` | `290.97 MB` | [DataStructuresBench.moka](moka/DataStructuresBench.moka) / [DataStructuresBench.java](java/DataStructuresBench.java) |

### `DataStructuresBench` Breakdown (1,000,000 operations each)

| Structure | Workload | Moka (unboxed) | Java (`Integer`) |
|---|---|---:|---:|
| `ArrayList` | 1M `add` + 1M `get` + 1M `remove(last)` | `32 ms` | `34 ms` |
| `HashMap` | 1M `put` + 1M `get` + 1M `remove` | `420 ms` | `158 ms` |
| `TreeMap` | 1M `put` + 1M `getOrDefault` + 1M `remove` | `1084 ms` | `654 ms` |
| `PriorityQueue` | 1M `offer` + 1M `peek` + 1M `poll` | `357 ms` | `675 ms` |

---

## Notes

- **Cold Start (`Startup`)**: Moka compiles to a standalone native binary (`~28x` faster startup than JVM initialization).
- **CPU Throughput (`FibCpu`)**: With `-O2`, Moka completes `fib(42)` in `487.0 ms` vs `1183.2 ms` on OpenJDK (`~2.4x` faster).
- **Memory & Monomorphized Generics (`ListInt`, `DataStructuresBench`)**: Moka's monomorphized collections store unboxed primitives (`int32_t`) contiguously, using `~2.46x` less peak memory (`118.40 MB` vs `290.97 MB` in `DataStructuresBench`) and making `PriorityQueue<int>` nearly `2x` faster than Java (`357 ms` vs `675 ms`). On `HashMap`/`TreeMap` with 1M allocations/rotations, HotSpot's JIT and generational GC remain faster on raw throughput.
- **Garbage Collection (`GcStress`)**: For short-lived workloads (500k allocations), Moka finishes in `23.5 ms` vs `54.9 ms` for Java (which includes JVM startup/JIT overhead; on long-running multi-threaded allocation-heavy workloads, Java's generational GC and TLABs scale better than Moka's global Stop-the-World Mark-and-Sweep).
