# Moka Stage 1 (Compilatore Self-Hosting in Moka)

`stage1` è il compilatore **self-hosting** di Moka, scritto interamente in Moka (`src/*.moka`) e dotato di runtime autonomo (`runtime/runtime.h`) e libreria standard (`std/`).

---

## Caratteristiche principali

- **Self-Hosting**: compilato da [`stage0`](../stage0/README.md) in `build/mokac`, è in grado di compilare se stesso (`mokac_s2`) generando codice C11 identico byte-per-byte (**bootstrap fixed-point**).
- **Mark-and-Sweep Garbage Collector**: runtime autonomo in [`runtime/runtime.h`](runtime/runtime.h) con **Shadow Stack** (`RtShadowFrame`), descrittori di tipo (`RtTypeInfo` con `offsetof`), attraversamento di istanze e `OBJ_REF_ARRAY`, e safepoint sui cicli `while`/`for` ([dettagli](../docs/it/internals.md)).
- **Compilazione diretta ad eseguibile**: `build/mokac` invoca automaticamente `cc -std=c11 -Wall -Wextra -Werror` producendo un binario standalone (oppure solo `.c` con `--emit-c` o `-o file.c`).
- **Auto-rilevamento di `std/` e `runtime/`**: include automaticamente `std/String.moka`, `std/Console.moka`, `std/ByteBuffer.moka`, `std/Io.moka` (disattivabile con `--no-stdlib`).
- **Estensioni del linguaggio rispetto a Stage 0**:
  - Cicli `for (init; cond; step)` e `switch / case / default`
  - Operatori `++`, `--` (prefissi e postfissi) e assegnamenti composti (`+=`, `-=`, `*=`, `/=`, `%=`)
  - Operatori bitwise e shift (`&`, `|`, `^`, `~`, `<<`, `>>`) e cast `(int)` / `(byte)`
  - **Ereditarietà singola (`extends`)**, `super(...)`, `super.metodo(...)`, `super.campo` e **polimorfismo via VTable** ([dettagli](../docs/it/internals.md))

---

## Struttura

- [`runtime/runtime.h`](runtime/runtime.h): GC Mark-and-Sweep, Shadow Stack, `RtArray`, `RtTypeInfo` e helper `native`.
- [`std/`](std): `String.moka`, `Console.moka`, `ByteBuffer.moka`, `Io.moka`.
- [`src/`](src): `Token.moka`, `Lexer.moka`, `Ast.moka`, `Parser.moka`, `Sema.moka`, `Codegen.moka`, `Main.moka`.
- [`examples/`](examples): `linked_list.moka`, `word_count.moka`.
- [`tests/`](tests): `test_std.moka`, `test_lexer.moka`, `hello_console.moka`, `test_gc.moka`, `test_stage1_features.moka`, `test_inheritance.moka`.

---

## Build, Utilizzo e Test

```bash
# Compila Stage 1 tramite Stage 0 ed esegue tutti i test (incluso GC, ereditarietà e fixed-point)
make all

# Compila un programma Moka direttamente in eseguibile (std/ e runtime/ inclusi automaticamente)
build/mokac -o build/linked_list examples/linked_list.moka
./build/linked_list

# Emette solo il file C11
build/mokac --emit-c examples/linked_list.moka -o build/linked_list.c
```

Opzioni CLI di `mokac`:
- `-o <output>`: percorso dell'eseguibile (o del `.c` se termina con `.c`)
- `--emit-c`: emette solo il codice C11 senza invocare `cc`
- `--keep-c`: conserva il file `.c` temporaneo dopo la compilazione
- `--no-stdlib`: non include automaticamente i file in `std/`
- `--runtime <header>` / `--runtime-dir <dir>`: personalizza header e path del runtime C
