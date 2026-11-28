# Benchmark Moka vs Java

> 🌐 **Lingua**: [English](README.md) | **Italiano**

Questa directory contiene una suite di benchmark comparativi tra **Moka** (`mokac` verso C11 con `-O2`) e **Java** (`javac` + `java` su OpenJDK `21`).

## Come Eseguirli

```bash
python3 benchmarks/run_benchmarks.py
```

Lo script compila ogni programma `.moka` e `.java` (usando `stage1/build/mokac` se presente, altrimenti `mokac` dal `PATH`), esegue 3 iterazioni per caso tramite `os.wait4` e riporta la **mediana** di tempo wall-clock e picco di memoria fisica (RSS).

---

## Risultati

| Benchmark | Descrizione | Tempo Moka | Tempo Java | Picco RSS Moka | Picco RSS Java | Sorgenti |
|---|---|---:|---:|---:|---:|---|
| **Startup** | Hello World minimale (cold start) | `1.1 ms` | `31.4 ms` | `13.51 MB` | `42.12 MB` | [Startup.moka](moka/Startup.moka) / [Startup.java](java/Startup.java) |
| **FibCpu** | `fib(42)` ricorsivo | `487.0 ms` | `1183.2 ms` | `13.51 MB` | `42.87 MB` | [FibCpu.moka](moka/FibCpu.moka) / [FibCpu.java](java/FibCpu.java) |
| **ListInt** | `1.000.000` di `int` in `ArrayList` + somma | `20.6 ms` | `66.9 ms` | `13.51 MB` | `76.12 MB` | [ListInt.moka](moka/ListInt.moka) / [ListInt.java](java/ListInt.java) |
| **GcStress** | `500.000` allocazioni temporanee di `Node` + `1.000` nodi vivi | `23.5 ms` | `54.9 ms` | `13.51 MB` | `55.91 MB` | [GcStress.moka](moka/GcStress.moka) / [GcStress.java](java/GcStress.java) |
| **DataStructuresBench** | `1.000.000` operazioni (insert/read/remove) su `ArrayList`, `HashMap`, `TreeMap`, `PriorityQueue` | `1934.2 ms` | `1599.8 ms` | `118.40 MB` | `290.97 MB` | [DataStructuresBench.moka](moka/DataStructuresBench.moka) / [DataStructuresBench.java](java/DataStructuresBench.java) |

### Dettaglio `DataStructuresBench` (1.000.000 operazioni ciascuna)

| Struttura | Carico di lavoro | Moka (unboxed) | Java (`Integer`) |
|---|---|---:|---:|
| `ArrayList` | 1M `add` + 1M `get` + 1M `remove(last)` | `32 ms` | `34 ms` |
| `HashMap` | 1M `put` + 1M `get` + 1M `remove` | `420 ms` | `158 ms` |
| `TreeMap` | 1M `put` + 1M `getOrDefault` + 1M `remove` | `1084 ms` | `654 ms` |
| `PriorityQueue` | 1M `offer` + 1M `peek` + 1M `poll` | `357 ms` | `675 ms` |

---

## Note

- **Cold Start (`Startup`)**: Moka produce un binario nativo standalone (avvio `~28x` più rapido rispetto all'inizializzazione della JVM).
- **Throughput CPU (`FibCpu`)**: con `-O2`, Moka completa `fib(42)` in `487.0 ms` contro `1183.2 ms` su OpenJDK (`~2.4x` più veloce).
- **Memoria e Generici Monomorfizzati (`ListInt`, `DataStructuresBench`)**: le collezioni monomorfizzate di Moka memorizzano tipi primitivi (`int32_t`) senza boxing, riducendo il picco di memoria di `~2.46x` (`118.40 MB` vs `290.97 MB` in `DataStructuresBench`) e rendendo `PriorityQueue<int>` quasi `2x` più veloce di Java (`357 ms` vs `675 ms`). Su `HashMap`/`TreeMap` con 1M di allocazioni/rotazioni, il JIT HotSpot e il GC generazionale restano più rapidi sul throughput puro.
- **Garbage Collection (`GcStress`)**: su carichi brevi da 500k allocazioni, Moka termina in `23.5 ms` contro `54.9 ms` di Java (che include avvio JVM e JIT; su programmi long-running multi-thread ad altissima frequenza di allocazione, il GC generazionale + TLAB della JVM scala meglio del Mark-and-Sweep globale di Moka).
