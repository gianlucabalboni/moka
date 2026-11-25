# Milestone 6 & 7: CLI `mokac`, `std/String.moka` e Test End-to-End

Con queste ultime due milestone il compilatore **Stage 0 (`mokac`)** è completo e funzionante da riga di comando.

---

## 1. Libreria standard minima (`std/String.moka`)

In [std/String.moka](../std/String.moka) la classe `String` è implementata interamente in Moka attorno a un array `byte[] data` UTF-8:

- **Metodi base**: `length()`, `byteAt(int i)`, `bytes()`.
- **Operazioni**: `equals(String other)`, `concat(String other)`, `substring(int start, int end)`, `String.fromInt(int n)`.
- **Output `native`**: `print()` e `println()` delegano direttamente a `rt_print_bytes` e `rt_println_bytes` definiti in [src/runtime.h](../src/runtime.h).

---

## 2. Driver CLI (`src/main.c`)

Il binario `build/mokac` accetta uno o più file `.moka` come singola unità di compilazione:

```bash
./build/mokac std/String.moka examples/hello.moka -o build/hello.c
cc -std=c11 -Wall -Wextra -Werror -Isrc build/hello.c -o build/hello
./build/hello arg1 arg2
```

Opzioni supportate:
- `-o <output.c>`: percorso del file C11 generato (oppure `-` per `stdout`; default `out.c`).
- `--runtime <header>`: percorso personalizzato per `#include "..."` (default `runtime.h`).
- `-h` / `--help`: mostra la sintassi di utilizzo.

---

## 3. Nota sull'interoperabilità `int` ↔ `byte` in Stage 0

In `String.fromInt` è necessario scrivere `tmp[count] = '0' + digit;` dove `'0' + digit` ha tipo `int` e `tmp[count]` è `byte`. Poiché Stage 0 non prevede ancora l'operatore di cast `(byte)`, in [src/sema.c](../src/sema.c) `int` e `byte` sono mutualmente assegnabili come tipi numerici (mappandosi in C a `int32_t` e `uint8_t`).

---

## 4. Verifica completa (`make all`)

Il comando `make all` compila `build/mokac` ed esegue:
1. `test-runtime`
2. `test-lexer`
3. `test-parser`
4. `test-sema`
5. `test-codegen`
6. `test-e2e`: compila [std/String.moka](../std/String.moka) + [examples/hello.moka](../examples/hello.moka) con `mokac` → compila `build/hello.c` con `-std=c11 -Wall -Wextra -Werror` → esegue `./build/hello arg1 arg2`.
