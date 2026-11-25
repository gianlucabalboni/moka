# Mark-and-Sweep Garbage Collector con Shadow Stack (`stage1`)

Lo **Stage 1** sostituisce l'Arena Allocator con un **Mark-and-Sweep Garbage Collector** basato su **Shadow Stack**, integrato tra [stage1/runtime/runtime.h](../runtime/runtime.h) e [stage1/src/Codegen.moka](../src/Codegen.moka).

---

## 1. Layout degli oggetti e Descrittori di Tipo (`RtTypeInfo`)

Ogni oggetto allocato con `calloc` inizia con `Obj header`:
- `type`: `OBJ_INSTANCE`, `OBJ_VAL_ARRAY`, `OBJ_REF_ARRAY`.
- `marked`: flag usato durante la fase di mark.
- `type_info`: puntatore a `RtTypeInfo` (per `OBJ_INSTANCE`), contenente `ptr_count` e l'array `ptr_offsets` generato tramite `offsetof(ClassName, field)` da `Codegen.moka`.
- `next`: collega l'oggetto nella lista globale `rt_all_objects`.

Per gli array di riferimenti (`OBJ_REF_ARRAY`, es. `String[]`, `Node[]`), il GC attraversa tutti gli elementi in `data[]`. Per gli array di primitivi (`OBJ_VAL_ARRAY`, es. `int[]`, `byte[]`), non effettua alcun attraversamento.

---

## 2. Shadow Stack e Safepoint

Per ogni metodo/costruttore che possiede `this`, parametri riferimento o variabili locali riferimento, `Codegen.moka` emette un `RtShadowFrame` sullo stack C:

1. **Prologo**: alloca `void* _gc_slots[N]` (azzerato), registra `&this`, `&param` e collega `_gc_frame` a `rt_shadow_stack`.
2. **Variabili locali**: ogni `VAR_DECL` di tipo riferimento registra `&var` nello slot assegnato `_gc_slots[k] = &var;`.
3. **Uscita dai blocchi / `break` / `continue`**: gli slot dei blocchi usciti vengono azzerati (`NULL`) per evitare puntatori pendenti.
4. **Ritorno**: il valore di ritorno viene salvato in `_gc_ret`, poi `rt_shadow_stack = _gc_frame.prev;` ripristina il frame precedente prima di `return`.
5. **Safepoint (`rt_gc_maybe_collect()`)**: chiamato all'ingresso delle funzioni e all'inizio di ogni iterazione `while`, dove non esistono temporanei non radicati in espressioni C annidate.

---

## 3. Verifica (`test-gc` e `test-bootstrap`)

- [tests/test_gc.moka](../tests/test_gc.moka): mantiene una lista collegata di 50 `Node` + un `String[]` vivi mentre alloca 10.000+ oggetti temporanei. Verifica che il GC esegua raccolte automatiche (`GC collections: 496`, `GC freed objs: 79999`) lasciando intatti tutti gli oggetti raggiungibili.
- `test-bootstrap`: `mokac_s2` (eseguito con il GC attivo!) compila se stesso producendo `stage1_gen2.c` identico a `stage1_gen1.c`.
