# Moka

> 🌐 **Language**: **English** | [Italiano](README.it.md)

**Moka** is a statically typed, Java-inspired programming language that transpiles to **standard C11** (`-std=c11 -pthread -lm`) with zero external dependencies.

The compiler follows a two-stage **self-hosting bootstrap** architecture:

1. **[`stage0/`](stage0/README.md)** — Minimal bootstrap compiler written in **C11** (`stage0/src/*.c`), using an Arena Allocator and supporting the core language subset.
2. **[`stage1/`](stage1/README.md)** — Full **self-hosting** compiler written in **Moka** (`stage1/src/*.moka`), featuring:
   - **Primitive types**: `int`, `long`, `float`, `double`, `byte`, `char`, `boolean` + `default(T)`
   - **OOP**: single inheritance (`extends`), `super`, VTable polymorphism, `interface` / `implements` (fat pointers), `abstract`, `final`, `static class`, method & constructor overloading, `instanceof` & checked downcasting
   - **Strings**: immutable `+` / `+=` / `concat()`, mutable `append()`, `charAt()`, `toInt()` / `toLong()` / `toDouble()` / `toFloat()`
   - **Exceptions**: `try / catch / finally / throw` (`setjmp`/`longjmp` with GC shadow stack unwinding)
   - **Generics via Monomorphization**: generic classes, interfaces, and methods (including primitive type arguments like `Box<int>`, diamond `new Box<>()`, and type inference)
   - **Concurrency**: `Thread`, `Runnable`, `Mutex`, `synchronized` blocks/methods with a cooperative **Stop-the-World Mark-and-Sweep GC**
   - **Modules & Standard Library**: `import`, `Console`, `Io`, `ByteBuffer`, `Exception`, `Math`, and `std/datastructures/*` (`List`, `ArrayList`, `Stack`, `Queue`, `Map`, `StringMap`)

---

## Quick Start

```bash
# 1. Build and test Stage 0 (C11 bootstrap compiler)
make -C stage0 all

# 2. Build Stage 1 via Stage 0, run all 18 test suites, and verify bootstrap fixed-point
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
├── docs/
│   ├── en/                # English language & stdlib documentation
│   └── it/                # Italian language & stdlib documentation
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
