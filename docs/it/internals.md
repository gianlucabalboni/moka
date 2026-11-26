# Architettura Interna del Compilatore (Stage 0 & Stage 1)

> 🌐 **Lingua**: [English](../en/internals.md) | **Italiano**

## 1. Pipeline di Bootstrap in Due Stadi

Moka utilizza un bootstrap self-hosting in due stadi:

1. **Stage 0 (`stage0/`)**: compilatore bootstrap minimale scritto in **C11** (`stage0/src/*.c`).
   - Usa un **Arena Allocator (`RtArena`)** in `stage0/src/runtime.h`, lexer zero-copy (`start`, `length`, `line`, `col`), parser recursive-descent con lookahead a 3 token, analisi semantica a 2 passaggi e codegen C11.
   - Compila il subset core (`int`, `byte`, `boolean`, `void`, costruttore singolo, `this.` esplicito, `if/else`, `while`).
   - Poiché `stage0` compila `stage1/src/*.moka`, il sorgente del compilatore Stage 1 resta nel subset Stage 0 e usa [`stage1/std_boot/`](../../stage1/std_boot/String.moka).
2. **Stage 1 (`stage1/`)**: compilatore self-hosting completo scritto in **Moka** (`stage1/src/*.moka`).
   - Verificato tramite punto fisso (`test-bootstrap`): `stage1` compila se stesso in `stage1_gen1.c` (`mokac_s2`), che compila se stesso in `stage1_gen2.c`, e `diff -u stage1_gen1.c stage1_gen2.c` deve essere vuoto.

---

## 2. Layout degli Oggetti e Garbage Collector (`stage1/runtime/runtime.h`)

### Header `Obj` e `RtTypeInfo`
Ogni oggetto allocato con `calloc` inizia con `Obj header`:
- `type`: `OBJ_INSTANCE`, `OBJ_VAL_ARRAY`, `OBJ_REF_ARRAY`, `OBJ_IFACE_ARRAY`.
- `marked`: flag usato nella fase di mark.
- `type_info`: puntatore a `RtTypeInfo` con `ptr_count`, `ptr_offsets` (generato con `offsetof(ClassName, field)`), `vtable`, `super`, `iface_count`, `interfaces` e `iface_vtables`.
- `next`: collega l'oggetto nella lista globale `rt_all_objects`.

### Shadow Stack e Safepoint
Per ogni metodo/costruttore con `this`, parametri riferimento o variabili locali riferimento, `Codegen.moka` emette un `RtShadowFrame` sullo stack C:
1. **Prologo**: alloca `void* _gc_slots[N]`, registra `&this` e gli indirizzi dei parametri riferimento, e collega `_gc_frame` a `_Thread_local rt_shadow_stack`.
2. **Variabili locali**: ogni `VAR_DECL` riferimento associa `&var` allo slot `_gc_slots[k]`.
3. **Uscita dai blocchi / cicli**: gli slot usciti dallo scope vengono azzerati (`NULL`) per evitare puntatori pendenti.
4. **Ritorno**: il valore viene salvato in `_gc_ret`, i frame di eccezione/monitor vengono srotolati (`emitExcUnwind()`), e `rt_shadow_stack = _gc_frame.prev;` ripristina il frame chiamante.
5. **Safepoint**: `rt_gc_maybe_collect()` viene emesso in testa ai cicli `while`/`for` (mai dentro espressioni C annidate dove potrebbero esserci temporanei non radicati nei registri).

---

## 3. Ereditarietà, VTable e Interfacce

### Struct Embedding in C11
Le struct vengono emesse in ordine di profondità di ereditarietà:
- Le classi base iniziano con `Obj header;`.
- Le sottoclassi (`class Dog extends Animal`) includono `Animal base;` all'offset `0`, rendendo `Dog*` compatibile in memoria con `Animal*`.
- I costruttori si dividono in `Class_init(this, ...)` (concatenato con `Super_init`) e `Class_new(...)` (`rt_alloc_instance` + `Class_init`).

### Dispatch Virtuale e Interfacce
- Ogni classe genera `ClassName_VTable` e dispatcher `static inline ClassName__call_method(...)` che valutano il ricevitore una sola volta con `rt_null_check`.
- `super.method(...)` risolve staticamente (`SuperClass_method((SuperClass*)this, ...)`).
- Le interfacce usano **fat pointer** `{ void* obj; const Iface_VTable* vtable; }`. Le interfacce ereditate sono incluse in `RtTypeInfo`, così `(Iface) baseRef` risolve dinamicamente la vtable della sottoclasse concreta tramite `rt_cast_iface_vtable`.

---

## 4. Generici (Monomorfizzazione) e Concorrenza

- **Monomorfizzazione (`Sema.moka`)**: classi generiche (`Box<T>`), interfacce (`List<T>`) e metodi (`identity<T>(T)`) vengono clonati e specializzati on-demand per ogni lista di argomenti di tipo concreti (`Box__int`, `Box__String`), saltando i template non istanziati in `Codegen.moka`.
- **GC Stop-the-World & `RtMonitor`**: `rt_gc_collect()` coordina tutti i thread tramite `rt_stw_mutex` / `rt_stw_cond` (`atomic_int rt_stw_requested`), mentre le chiamate bloccanti (`join`, `sleep`, `Mutex.lock`, `RtMonitor`) impostano `in_native_wait = 1` per evitare qualsiasi deadlock con il GC.
