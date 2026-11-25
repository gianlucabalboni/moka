# Milestone 5: Code Generator C11 (`src/codegen.h`, `src/codegen.c`)

Il code generator (`codegen_emit`) visita l'AST annotato da `sema.c` ed emette codice **C11 standard** leggibile, privo di warning anche con `-Wall -Wextra -Werror`.

---

## 1. Ordine di emissione nel file `.c` generato

1. **`#include "runtime.h"`**: include arena, `Obj`, `RtArray` e funzioni `native`.
2. **Forward declarations**: `typedef struct ClassName ClassName;` per ogni classe, risolvendo i riferimenti circolari tra struct.
3. **Definizioni `struct`**: ogni classe inizia con `Obj header;`, seguito dai suoi campi mappati in C (`int` → `int32_t`, `byte` → `uint8_t`, `boolean` → `bool`, classi → `ClassName*`, array → `RtArray*`).
4. **Prototipi delle funzioni**: per tutti i costruttori e metodi non-`native`.
5. **Definizioni delle funzioni**:
   - **Costruttori (`ClassName_new(...)`)**: allocano con `rt_alloc_obj(sizeof(ClassName), OBJ_INSTANCE)`, eseguono il corpo e restituiscono `this` (anche in caso di `return;` esplicito).
   - **Metodi d'istanza (`ClassName_method(ClassName* this, ...)`)**: ricevono `this` come primo parametro.
   - **Metodi `static`**: nessuna `this`, chiamati come `ClassName_method(...)`.
   - **Metodi `native`**: nessuna definizione emessa; la chiamata viene emessa direttamente come `method_name(...)` verso `runtime.h`.
6. **Entry point `main(int argc, char** argv)`**: se esiste `Main.main()`, inizializza il runtime con `rt_init(argc, argv)`, invoca `Main_main()`, chiama `rt_shutdown()` e restituisce il codice d'uscita.

---

## 2. Sicurezza a runtime & String literal

- **String literal**: `"hello"` viene emesso come `String_new(rt_byte_array_from_literal("hello", 5))` con escape ottale sicuro per byte non stampabili.
- **Accesso agli array (`rt_array_elem_ptr`)**: in [src/runtime.h](../src/runtime.h), `arr[i]` diventa `(*(T*)rt_array_elem_ptr(arr, i))`, che controlla sia `NULL` che i limiti (`0 <= i < length`) valutando `arr` e `i` una sola volta, sia in lettura che in scrittura.
- **Null check**: accessi a campo, `.length` e chiamate su istanza passano da `rt_null_check(...)`.
- **Zero-initialization**: le variabili locali dichiarate senza inizializzatore vengono inizializzate a `0` / `false` / `NULL`.

---

## 3. Verifica

Il test end-to-end in [tests/test_codegen.c](../tests/test_codegen.c) compila un programma Moka completo (con `String`, `fib(10)`, array e `native` I/O) in `build/gen_test.c`, lo compila con `cc -std=c11 -Wall -Wextra -Werror` e lo esegue verificando l'output (`Hello Moka!` e `55`).
