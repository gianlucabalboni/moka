# CLI, Installazione e Architettura Runtime

> 🌐 **Lingua**: [English](../en/cli-and-runtime.md) | **Italiano**

## Utilizzo della CLI `mokac`

```text
Usage: mokac <file1.moka> [file2.moka ...] [-o <output>] [-r|--run] [--emit-c] [--keep-c] [--no-stdlib] [--std-dir <dir>] [--runtime <header>] [--runtime-dir <dir>]
```

| Opzione | Descrizione |
|---|---|
| `-o <output>` | Percorso del file eseguibile (o `.c` se termina con `.c`) |
| `-r`, `--run` | Compila in un binario temporaneo, esegue subito e pulisce il binario |
| `--emit-c` | Genera solo il sorgente C11 senza invocare `cc` |
| `--keep-c` | Mantiene il file `.c` intermedio dopo la compilazione |
| `--no-stdlib` | Non carica automaticamente `std/*.moka` |
| `--cc <compiler>` | Specifica il compilatore C11 (`cc` su POSIX, `gcc` su Windows di default) |
| `--std-dir <dir>` | Specifica la directory della libreria standard |
| `--runtime-dir <dir>` | Specifica la directory contenente `runtime.h` |

### Installazione e Rimozione Globale

```bash
# Installa in ~/.local/bin/mokac e ~/.local/share/moka/
make -C stage1 install

# Rimuove completamente Moka dal sistema
make -C stage1 uninstall

# Cross-compilazione pacchetto Windows (richiede x86_64-w64-mingw32-gcc)
make -C stage1 windows
```

---

## Architettura Runtime (`stage1/runtime/runtime.h`)

1. **Garbage Collector Mark-and-Sweep Stop-the-World**:
   - Ogni thread mantiene un proprio **Shadow Stack** (`RtShadowFrame`) e **Exception Stack** (`RtExceptionFrame`) in memoria `_Thread_local`, registrati in `rt_thread_list`.
   - I safepoint vengono emessi all'inizio dei cicli (`rt_gc_maybe_collect()`).
   - Le operazioni bloccanti (`Thread.join`, `Thread.sleep`, `Mutex.lock`, attesa su `RtMonitor`) impostano `in_native_wait = 1`, evitando qualsiasi deadlock durante una raccolta STW.
2. **Eccezioni (`setjmp` / `longjmp`)**:
   - `rt_throw()` ripristina `rt_shadow_stack` alla profondità salvata del frame `RtExceptionFrame` di destinazione prima del `longjmp()`, evitando frame orfani durante il GC.
   - I blocchi `finally` e i monitor `synchronized` vengono rilasciati in modo garantito su uscita normale, `return` e `throw`.
3. **Monitor Rientranti sugli Oggetti (`RtMonitor`)**:
   - `synchronized (obj)` associa on-demand un `RtMonitor` in una tabella dedicata (zero overhead di memoria sugli oggetti normali) e traccia `(owner, depth)` per la rientranza stile Java.
