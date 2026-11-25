# Milestone 1: Runtime & Memory Management (`src/runtime.h`)

Il file [src/runtime.h](../src/runtime.h) è una libreria **single-header** in C11 che verrà inclusa automaticamente da ogni file C generato dal compilatore Moka (`mokac`), oltre a fornire l'Arena Allocator riutilizzabile dal compilatore stesso.

---

## 1. Arena Allocator (`RtArena`)

In Stage 0 non usiamo `malloc`/`free` per ogni singolo oggetto o nodo AST, ma un **Arena Allocator** a blocchi:

- **`RtArenaBlock`**: ogni blocco alloca almeno `MOKA_ARENA_DEFAULT_CAP` (1 MB) tramite `calloc(1, ...)` — quindi tutta la memoria è **già azzerata (zero-initialized)**. Se una singola allocazione supera 1 MB, viene creato un blocco dimensionato su misura.
- **`rt_align8(size)`**: arrotonda ogni richiesta al multiplo di 8 byte superiore (`(n + 7u) & ~(size_t)7u`), garantendo l'allineamento corretto su architetture a 64 bit per puntatori e `int32_t`.
- **`rt_arena_alloc(arena, size)`**: avanza semplicemente un offset (`used`) nel blocco corrente; se non c'è spazio, alloca un nuovo `RtArenaBlock` e lo collega in lista.
- **`rt_arena_free(arena)`**: libera l'intera catena di blocchi in un colpo solo a fine esecuzione (`rt_shutdown()`).

**Perché in Stage 0?** Velocità estrema, zero frammentazione, niente `free()` sparsi, e campi delle classi/array automaticamente inizializzati a `0`/`NULL`/`false`.

---

## 2. Layout degli oggetti GC-Ready (`Obj` e `RtArray`)

Ogni oggetto allocato sull'heap (istanze di classi Moka e array) inizia con lo stesso header `Obj`:

```c
typedef enum {
    OBJ_INSTANCE = 0,
    OBJ_VAL_ARRAY = 1, /* Elementi primitivi (int, byte, boolean) */
    OBJ_REF_ARRAY = 2  /* Elementi riferimento (Obj*) */
} ObjType;

typedef struct Obj {
    ObjType type;
    bool marked;
    struct Obj* next;
} Obj;
```

- **`type`**: distingue istanze di classe da array di valori primitivi (`OBJ_VAL_ARRAY`) e array di puntatori (`OBJ_REF_ARRAY`). Questo permetterà al futuro Garbage Collector di sapere se deve attraversare `data[]` cercando puntatori oppure no.
- **`marked` e `next`**: ogni chiamata a `rt_alloc_obj` inserisce il nuovo oggetto in testa alla lista globale `rt_all_objects`. Quando in futuro sostituiremo l'Arena con un **Mark-and-Sweep GC**, il layout delle struct generate non dovrà cambiare.

### Array (`RtArray`)

```c
typedef struct RtArray {
    Obj header;
    int32_t length;
    int32_t elem_size;
    uint8_t data[];
} RtArray;
```

- **`data[]`** (C11 flexible array member) memorizza gli elementi subito dopo l'header, in un'unica allocazione contigua.
- **`rt_array_new(length, elem_size, is_ref)`**: verifica che `length >= 0` e alloca `sizeof(RtArray) + length * elem_size`.
- **`rt_byte_array_from_literal(str, len)`**: usata dal codegen per trasformare un literal Moka `"hello"` in un `byte[]` da passare a `String_new(...)`.
- **`rt_bounds_check(arr, index)`** e **`rt_null_check(ptr, context)`**: controlli a runtime per intercettare subito indici fuori dai limiti o dereferenziazioni `null`.

---

## 3. Helper `native` di I/O e sistema

Per mantenere `String` fuori dal runtime C, le funzioni `native` lavorano direttamente su `RtArray*` (che in Moka corrisponde a `byte[]`):

- **Output**: `rt_print_int`, `rt_print_bool`, `rt_print_bytes`, `rt_println_bytes`, `rt_eprint_bytes`.
- **Argomenti CLI**: `rt_argc()` e `rt_argv_bytes(index)` (restituisce `byte[]` così Moka può costruirci una `String`).
- **File I/O**: `rt_read_file_bytes(path_bytes)` legge un intero file in un `byte[]`; `rt_write_file_bytes(path_bytes, data_bytes)` scrive un `byte[]` su disco.
- **Uscita**: `rt_exit(code)`.

---

## 4. Verifica

I test in [tests/test_runtime.c](../tests/test_runtime.c) verificano:
1. Creazione di `byte[]` da literal (`OBJ_VAL_ARRAY`).
2. Creazione di array di riferimenti (`OBJ_REF_ARRAY`) e zero-initialization.
3. Allineamento a 8 byte delle istanze e concatenamento nella lista `rt_all_objects`.
4. `rt_bounds_check` e lettura argomenti CLI (`rt_argc`, `rt_argv_bytes`).
