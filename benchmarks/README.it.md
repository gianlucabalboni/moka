# Benchmark Moka vs Java

> 🌐 **Lingua**: [English](README.md) | **Italiano**

Questa directory contiene una suite di benchmark comparativi tra **Moka** (`mokac` verso C11 con `-O2`) e **Java** (`javac` + `java` su OpenJDK `21.0.13.1`).

## Come Eseguirli

```bash
python3 benchmarks/run_benchmarks.py
```

Lo script compila ogni programma `.moka` e `.java`, esegue 3 iterazioni per caso tramite `os.wait4` e riporta la **mediana** di tempo wall-clock e picco di memoria fisica (RSS).

---

## Risultati

| Benchmark | Descrizione | Tempo Moka | Tempo Java | Picco RSS Moka | Picco RSS Java | Sorgenti |
|---|---|---:|---:|---:|---:|---|
| **Startup** | Hello World minimale (cold start) | `1.4 ms` | `33.4 ms` | `13.79 MB` | `41.97 MB` | [Startup.moka](moka/Startup.moka) / [Startup.java](java/Startup.java) |
| **FibCpu** | `fib(42)` ricorsivo | `502.1 ms` | `1195.0 ms` | `13.79 MB` | `43.21 MB` | [FibCpu.moka](moka/FibCpu.moka) / [FibCpu.java](java/FibCpu.java) |
| **ListInt** | `1.000.000` di `int` in `ArrayList` + somma | `19.4 ms` | `72.9 ms` | `13.79 MB` | `76.02 MB` | [ListInt.moka](moka/ListInt.moka) / [ListInt.java](java/ListInt.java) |
| **GcStress** | `500.000` allocazioni temporanee di `Node` + `1.000` nodi vivi | `24.6 ms` | `60.1 ms` | `13.79 MB` | `55.71 MB` | [GcStress.moka](moka/GcStress.moka) / [GcStress.java](java/GcStress.java) |

---

## Note

- **Cold Start (`Startup`)**: Moka produce un binario nativo standalone (avvio `~24x` più rapido rispetto all'inizializzazione della JVM).
- **Throughput CPU (`FibCpu`)**: con `-O2`, Moka completa `fib(42)` in `502.1 ms` contro `1195.0 ms` su OpenJDK 21 (`~2.4x` più veloce).
- **Memoria e Generici Monomorfizzati (`ListInt`)**: `ArrayList<int>` in Moka memorizza `int32_t` contigui senza boxing (`13.79 MB` picco RSS, `19.4 ms`), mentre `ArrayList<Integer>` in Java alloca un oggetto `Integer` boxato per ogni elemento (`76.02 MB` picco RSS, `72.9 ms`).
- **Garbage Collection (`GcStress`)**: su carichi brevi da 500k allocazioni, Moka termina in `24.6 ms` contro `60.1 ms` di Java (che include avvio JVM e JIT; su programmi long-running multi-thread ad altissima frequenza di allocazione, il GC generazionale + TLAB della JVM scala meglio del Mark-and-Sweep globale di Moka).
