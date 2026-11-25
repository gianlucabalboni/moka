# Moka Stage 0 (Bootstrap Compiler in C11)

`stage0` è il compilatore bootstrap di Moka, scritto in **C11 standard** con zero dipendenze esterne (solo standard library C). Compila il subset core di Moka in codice C11 leggibile, usato per costruire il compilatore [Stage 1](../stage1/README.md).

---

## Architettura

- [`src/runtime.h`](src/runtime.h): header C11 single-file con Arena Allocator (`RtArena`), header `Obj` (`OBJ_INSTANCE`, `OBJ_VAL_ARRAY`, `OBJ_REF_ARRAY`), `RtArray` e funzioni `native` di I/O e sistema.
- [`src/lexer.h`](src/lexer.h) / [`src/lexer.c`](src/lexer.c): tokenizer zero-copy (`start`, `length`, `line`, `col`).
- [`src/ast.h`](src/ast.h) / [`src/parser.h`](src/parser.h) / [`src/parser.c`](src/parser.c): Recursive Descent Parser con lookahead a 3 token e nodi allocati nell'Arena.
- [`src/sema.h`](src/sema.h) / [`src/sema.c`](src/sema.c): analisi semantica a 2 passaggi (registrazione classi/firme + type-checking).
- [`src/codegen.h`](src/codegen.h) / [`src/codegen.c`](src/codegen.c): emissione C11 ordinata (`typedef`, `struct`, prototipi, costruttori `Class_new`, metodi, wrapper `main`).
- [`src/main.c`](src/main.c): driver CLI `mokac`.
- [`std/String.moka`](std/String.moka) e [`std/Console.moka`](std/Console.moka): libreria standard minima.

---

## Subset supportato (Stage 0)

- **Tipi**: `int` (`int32_t`), `byte` (`uint8_t`), `boolean`, `void`, classi definite dall'utente, array `T[]` (con `.length`), `null`.
- **Classi**: campi, singolo costruttore, metodi d'istanza (`this.` esplicito), metodi `static` e `static native`. Modificatori `public`/`private` parsati.
- **Controllo di flusso**: `if / else`, `while`, `break`, `continue`, `return`.

---

## Build e Test

```bash
# Compila build/mokac ed esegue tutti i test (runtime, lexer, parser, sema, codegen, e2e)
make all

# Compila manualmente un programma Moka in C11
./build/mokac std/String.moka std/Console.moka examples/hello.moka -o build/hello.c
cc -std=c11 -Wall -Wextra -Werror -Isrc build/hello.c -o build/hello
./build/hello
```

---

## Documentazione delle Milestone

1. [Runtime & Memory Management](docs/01-runtime.md)
2. [Lexer](docs/02-lexer.md)
3. [AST & Parser](docs/03-parser.md)
4. [Analisi Semantica](docs/04-sema.md)
5. [Codegen C11](docs/05-codegen.md)
6. [CLI `mokac` & `std/String.moka`](docs/06-cli-and-stdlib.md)
