# Moka

> 🌐 **Lingua**: [English](README.md) | **Italiano**

**Moka** è un linguaggio di programmazione staticamente tipizzato, con sintassi ispirata a Java, che traspila in **C11 standard** (`-std=c11 -pthread -lm`) senza dipendenze esterne.

Il progetto segue una strategia di **bootstrapping (self-hosting)** in due stadi:

1. **[`stage0/`](stage0/README.md)** — Compilatore bootstrap minimale scritto in **C11** (`stage0/src/*.c`), con Arena Allocator e supporto al subset core del linguaggio.
2. **[`stage1/`](stage1/README.md)** — Compilatore **self-hosting** completo scritto in **Moka** (`stage1/src/*.moka`), dotato di:
   - **Tipi primitivi**: `int`, `long`, `float`, `double`, `byte`, `char`, `boolean` + `default(T)`
   - **OOP**: ereditarietà singola (`extends`), `super`, polimorfismo tramite VTable, `interface` / `implements` (fat pointer), `abstract`, `final`, `static class`, overloading di metodi e costruttori, `instanceof` e downcast controllati
   - **Stringhe**: `+` / `+=` / `concat()` immutabili, `append()` mutabile, `charAt()`, `toInt()` / `toLong()` / `toDouble()` / `toFloat()`
   - **Eccezioni**: `try / catch / finally / throw` (`setjmp`/`longjmp` con unwinding dello shadow stack GC)
   - **Generici tramite Monomorfizzazione**: classi, interfacce e metodi generici (funzionanti anche sui primitivi come `Box<int>`, sintassi diamond `new Box<>()` e inferenza)
   - **Concorrenza**: `Thread`, `Runnable`, `Mutex`, blocchi e metodi `synchronized` con **Garbage Collector Mark-and-Sweep Stop-the-World**
   - **Moduli e Libreria Standard**: `import`, `Console`, `Io`, `ByteBuffer`, `Exception`, `Math` e `std/datastructures/*` (`List`, `ArrayList`, `Stack`, `Queue`, `Map`, `StringMap`)

---

## Quick Start

```bash
# 1. Compila e testa Stage 0 (compilatore bootstrap in C11)
make -C stage0 all

# 2. Compila Stage 1 tramite Stage 0, esegui tutti i 18 test e verifica il bootstrap
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
├── docs/
│   ├── en/                # Documentazione linguaggio e stdlib in Inglese
│   └── it/                # Documentazione linguaggio e stdlib in Italiano
├── stage0/                # Compilatore bootstrap minimale in C11
└── stage1/                # Compilatore self-hosting, runtime, stdlib e test
    ├── Makefile
    ├── runtime/           # Runtime C11 (GC STW, eccezioni, pthread, monitor)
    ├── src/               # Sorgenti compilatore Stage 1
    ├── std/               # Libreria standard Stage 1 completa
    ├── std_boot/          # Stdlib compatibile Stage 0 per compilare stage1/src
    └── tests/             # Suite di test end-to-end
```
