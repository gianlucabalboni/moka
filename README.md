# Moka

**Moka** è un linguaggio di programmazione staticamente tipizzato con sintassi ispirata a Java che traspila in **C11 standard** senza dipendenze esterne.

Il progetto segue una strategia di **bootstrapping (self-hosting)** in due stadi:

1. **[`stage0/`](stage0/README.md)**: compilatore bootstrap minimale scritto in **C11** (`stage0/src/*.c`), con Arena Allocator e supporto al subset core del linguaggio.
2. **[`stage1/`](stage1/README.md)**: compilatore **self-hosting** scritto interamente in **Moka** (`stage1/src/*.moka`), dotato di:
   - **Mark-and-Sweep Garbage Collector** con Shadow Stack
   - Compilazione diretta verso eseguibile (invocando `cc`) o emissione C11 (`--emit-c`)
   - Auto-inclusione di `std/` (`String`, `Console`, `ByteBuffer`, `Io`)
   - Cicli `for`, `switch`/`case`/`default`, `++`/`--`, `+=`/`-=`/`*=`/`/=`/`%=`, operatori bitwise, cast `(int)`/`(byte)`
   - **Ereditarietà singola (`extends`)**, `super(...)`, `super.metodo(...)` e polimorfismo tramite **VTable**

---

## Quick Start

```bash
# 1. Compila e testa Stage 0 (compilatore in C11)
make -C stage0 all

# 2. Compila Stage 1 tramite Stage 0, esegui tutti i test e verifica il fixed-point di bootstrap
make -C stage1 all
```

### Compilare ed eseguire un programma Moka con Stage 1

```bash
cd stage1
build/mokac -o build/linked_list examples/linked_list.moka
./build/linked_list
```

---

## Struttura del repository

```text
moka/
├── README.md
├── stage0/          # Compilatore bootstrap in C11
│   ├── Makefile
│   ├── README.md
│   ├── docs/
│   ├── examples/
│   ├── src/
│   ├── std/
│   └── tests/
└── stage1/          # Compilatore self-hosting in Moka
    ├── Makefile
    ├── README.md
    ├── docs/
    ├── examples/
    ├── runtime/
    ├── src/
    ├── std/
    └── tests/
```
