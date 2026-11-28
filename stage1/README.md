# Moka Stage 1 (Compilatore Self-Hosting in Moka)

`stage1` è il compilatore **self-hosting** di Moka, scritto interamente in Moka (`src/*.moka`) e dotato di runtime autonomo (`runtime/runtime.h`) e libreria standard (`std/`).

---

## Caratteristiche principali

- **Self-Hosting**: compilato da [`stage0`](../stage0/README.md) in `build/mokac`, è in grado di compilare se stesso (`mokac_s2`) generando codice C11 identico byte-per-byte (**bootstrap fixed-point**).
- **Mark-and-Sweep Garbage Collector**: runtime autonomo in [`runtime/runtime.h`](runtime/runtime.h) con **Shadow Stack** (`RtShadowFrame`), descrittori di tipo (`RtTypeInfo` con `offsetof`), attraversamento di istanze e `OBJ_REF_ARRAY`, e safepoint sui cicli `while`/`for` ([dettagli](../docs/it/internals.md)).
- **Compilazione diretta ad eseguibile**: `build/mokac` invoca automaticamente `cc -std=c11 -Wall -Wextra -Werror` producendo un binario standalone (oppure solo `.c` con `--emit-c` o `-o file.c`).
- **Auto-rilevamento di `std/` e `runtime/`**: include automaticamente `std/String.moka`, `std/Console.moka`, `std/ByteBuffer.moka`, `std/Io.moka`, `std/Exception.moka`, `std/Runnable.moka`, `std/Thread.moka`, `std/Mutex.moka` (e i moduli importati come `std/Comparable`, `std/Comparator`, `std/Exceptions`, `std/datastructures/*`), disattivabile con `--no-stdlib`.
- **Estensioni del linguaggio rispetto a Stage 0**:
  - Cicli `for (init; cond; step)`, **`for (Type var : expr)` (`for-each` su array e `Iterable`/`Iterator`)** e `switch / case / default`
  - Operatori `++`, `--` (prefissi e postfissi) e assegnamenti composti (`+=`, `-=`, `*=`, `/=`, `%=`)
  - Operatori bitwise e shift (`&`, `|`, `^`, `~`, `<<`, `>>`) e cast `(int)` / `(byte)` / `(long)` / `(double)` / `(float)` / `(char)`
  - **OOP completo**: ereditarietà singola (`extends`), `super(...)`, `super.metodo(...)`, polimorfismo via VTable, `abstract`, `final`, `static class`, `instanceof` e downcast controllati
  - **Interfacce ed ereditarietà tra interfacce**: `interface A extends B, C` (fat pointers `{ obj, vtable }`)
  - **Generics per monomorphization**: classi, interfacce e metodi generici (anche con argomenti primitivi unboxed), diamond `<>` e type inference
  - **Eccezioni e Concorrenza**: `try / catch / finally / throw` (`setjmp`/`longjmp`), `Thread`, `Runnable`, `Mutex`, `synchronized`

---

## Struttura

- [`runtime/runtime.h`](runtime/runtime.h): GC Mark-and-Sweep STW, Shadow Stack, `RtArray`, `RtTypeInfo`, monitor/mutex e helper `native`.
- [`std/`](std): libreria standard completa (`String`, `Comparable`, `Comparator`, `Exceptions`, `Math`, `datastructures/*`, ecc.).
- [`std_boot/`](std_boot): sottoinsieme compatibile con Stage 0 usato per compilare `stage1/src`.
- [`src/`](src): `Token.moka`, `Lexer.moka`, `Ast.moka`, `Parser.moka`, `Sema.moka`, `Codegen.moka`, `Main.moka`.
- [`../examples/`](../examples): programmi di esempio.
- [`tests/`](tests): suite di test end-to-end (`make all`).

---

## Build, Utilizzo e Test

```bash
# Compila Stage 1 tramite Stage 0 ed esegue tutti i test (incluso GC, ereditarietà e fixed-point)
make all

# Compila un programma Moka direttamente in eseguibile (std/ e runtime/ inclusi automaticamente)
build/mokac -o build/linked_list ../examples/linked_list.moka
./build/linked_list

# Emette solo il file C11
build/mokac --emit-c ../examples/linked_list.moka -o build/linked_list.c
```

Opzioni CLI di `mokac`:
- `-o <output>`: percorso dell'eseguibile (o del `.c` se termina con `.c`)
- `--emit-c`: emette solo il codice C11 senza invocare `cc`
- `--keep-c`: conserva il file `.c` temporaneo dopo la compilazione
- `--no-stdlib`: non include automaticamente i file in `std/`
- `--runtime <header>` / `--runtime-dir <dir>`: personalizza header e path del runtime C
