# Moka Stage 1 — Compilatore Self-Hosting

Il compilatore **Stage 1 (`stage1/build/mokac`)** è scritto interamente in Moka (sul subset Stage 0) ed è in grado di compilare se stesso raggiungendo il **fixed-point di bootstrap** (`stage1_gen1.c == stage1_gen2.c`).

---

## Struttura di `stage1/`

- **Libreria standard (`stage1/std/`)**:
  - [String.moka](../std/String.moka): classe `String` su `byte[]` UTF-8.
  - [Console.moka](../std/Console.moka): `Console.print`, `Console.println`, `Console.eprint`, `Console.eprintln`, `Console.printInt`.
  - [ByteBuffer.moka](../std/ByteBuffer.moka): buffer dinamico di byte per l'emissione del codice C11.
  - [Io.moka](../std/Io.moka): wrapper per file I/O, argomenti CLI ed esecuzione comandi (`Io.exec`).
- **Compilatore (`stage1/src/`)**:
  - [Token.moka](../src/Token.moka) e [Lexer.moka](../src/Lexer.moka): tokenizzazione su `byte[]`.
  - [Ast.moka](../src/Ast.moka): nodi AST a tag (`kind`) e liste tipizzate (`ExprList`, `StmtList`, `ClassList`, ...).
  - [Parser.moka](../src/Parser.moka): Recursive Descent Parser con lookahead a 3 token.
  - [Sema.moka](../src/Sema.moka): analisi semantica a 2 passaggi.
  - [Codegen.moka](../src/Codegen.moka): generatore C11 su `ByteBuffer`.
  - [Main.moka](../src/Main.moka): driver CLI che invoca `cc` direttamente (oppure emette solo `.c` con `--emit-c` o `-o file.c`).

---

## Comandi principali

```bash
# Compila Stage 1 usando Stage 0 ed esegue tutti i test (incluso fixed-point bootstrap)
make -C stage1 all

# Compila un programma Moka direttamente in eseguibile usando Stage 1
./stage1/build/mokac --runtime-dir stage0/src stage1/std/String.moka stage1/std/Console.moka mio_file.moka -o mio_app
./mio_app

# Emette solo il file C11
./stage1/build/mokac --emit-c stage1/std/String.moka stage1/std/Console.moka mio_file.moka -o out.c
```
