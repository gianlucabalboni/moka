# CLI, Installation & Runtime Architecture

> 🌐 **Language**: **English** | [Italiano](../it/cli-and-runtime.md)

## `mokac` CLI Usage

```text
Usage: mokac <file1.moka> [file2.moka ...] [-o <output>] [-r|--run] [--emit-c] [--keep-c] [--no-stdlib] [--std-dir <dir>] [--runtime <header>] [--runtime-dir <dir>]
```

| Flag | Description |
|---|---|
| `-o <output>` | Output executable path (or `.c` file if ending in `.c`) |
| `-r`, `--run` | Compile to a temporary binary, run it immediately, and clean up |
| `--emit-c` | Emit C11 source only without invoking `cc` |
| `--keep-c` | Keep the intermediate `.c` file after building the executable |
| `--no-stdlib` | Do not auto-load `std/*.moka` |
| `--std-dir <dir>` | Specify the standard library directory |
| `--runtime-dir <dir>` | Specify the directory containing `runtime.h` |

### Global Installation & Removal

```bash
# Install to ~/.local/bin/mokac and ~/.local/share/moka/
make -C stage1 install

# Remove globally
make -C stage1 uninstall
```

---

## Runtime Architecture (`stage1/runtime/runtime.h`)

1. **Stop-the-World Mark-and-Sweep GC**:
   - Each thread maintains a `_Thread_local` **Shadow Stack** (`RtShadowFrame`) and **Exception Stack** (`RtExceptionFrame`), registered in `rt_thread_list`.
   - Safepoints are emitted at loop headers (`rt_gc_maybe_collect()`).
   - Blocking syscalls (`Thread.join`, `Thread.sleep`, `Mutex.lock`, `RtMonitor` entry) mark the thread as `in_native_wait = 1` so GC never deadlocks waiting on blocked threads.
2. **Exceptions (`setjmp` / `longjmp`)**:
   - `rt_throw()` restores `rt_shadow_stack` to the target `RtExceptionFrame`'s saved stack depth before `longjmp()`, preventing stale stack frames during GC.
   - `finally` blocks and `synchronized` monitors unwind cleanly on normal exit, `return`, and `throw`.
3. **Reentrant Object Monitors (`RtMonitor`)**:
   - `synchronized (obj)` looks up an on-demand `RtMonitor` in a monitor table (zero per-object memory overhead on regular objects) and tracks `(owner, depth)` for Java-style reentrancy.
