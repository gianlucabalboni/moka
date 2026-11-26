# Moka Compiler Internals (Stage 0 & Stage 1)

> 🌐 **Language**: **English** | [Italiano](../it/internals.md)

## 1. Two-Stage Self-Hosting Bootstrap

Moka is built with a two-stage bootstrap pipeline:

1. **Stage 0 (`stage0/`)**: minimal bootstrap compiler written in **C11** (`stage0/src/*.c`).
   - Uses an **Arena Allocator (`RtArena`)** in `stage0/src/runtime.h` with zero-copy lexer tokens (`start`, `length`, `line`, `col`), 3-token lookahead recursive-descent parser, 2-pass semantic analyzer, and C11 codegen.
   - Compiles the core subset (`int`, `byte`, `boolean`, `void`, single constructor, explicit `this.`, `if/else`, `while`).
   - Because `stage0` compiles `stage1/src/*.moka`, the Stage 1 compiler source itself stays within the Stage 0 subset and uses [`stage1/std_boot/`](../../stage1/std_boot/String.moka).
2. **Stage 1 (`stage1/`)**: full self-hosting compiler written in **Moka** (`stage1/src/*.moka`).
   - Verified via a fixed-point bootstrap check (`test-bootstrap`): `stage1` compiles itself into `stage1_gen1.c` (`mokac_s2`), which compiles itself into `stage1_gen2.c`, and `diff -u stage1_gen1.c stage1_gen2.c` must be empty.

---

## 2. Object Layout & Garbage Collector (`stage1/runtime/runtime.h`)

### Object Header & `RtTypeInfo`
Every heap object allocated with `calloc` begins with `Obj header`:
- `type`: `OBJ_INSTANCE`, `OBJ_VAL_ARRAY`, `OBJ_REF_ARRAY`, `OBJ_IFACE_ARRAY`.
- `marked`: mark bit used during Mark-and-Sweep.
- `type_info`: pointer to `RtTypeInfo` containing `ptr_count`, `ptr_offsets` (generated with `offsetof(ClassName, field)`), `vtable`, `super`, `iface_count`, `interfaces`, and `iface_vtables`.
- `next`: intrusive linked list node in `rt_all_objects`.

### Shadow Stack & Safepoints
For every method/constructor with `this`, reference parameters, or reference locals, `Codegen.moka` emits an `RtShadowFrame` on the C stack:
1. **Prologue**: allocates `void* _gc_slots[N]`, registers `&this` and reference parameter addresses, and links `_gc_frame` into `_Thread_local rt_shadow_stack`.
2. **Locals**: each reference `VAR_DECL` binds `&var` into `_gc_slots[k]`.
3. **Block / Loop Exits**: exited scope slots are cleared to `NULL` to avoid dangling references.
4. **Return**: return value is saved into `_gc_ret`, exception frames unwind (`emitExcUnwind()`), and `rt_shadow_stack = _gc_frame.prev;` restores the caller frame.
5. **Safepoints**: `rt_gc_maybe_collect()` is emitted at `while`/`for` loop headers (never inside nested C call arguments where unrooted register temporaries may exist).

---

## 3. Inheritance, VTables & Interfaces

### C11 Struct Embedding
Structs are emitted in order of inheritance depth:
- Root classes start with `Obj header;`.
- Subclasses (`class Dog extends Animal`) embed `Animal base;` at offset `0`, making `Dog*` layout-compatible with `Animal*`.
- Constructors split into `Class_init(this, ...)` (chained with `Super_init`) and `Class_new(...)` (`rt_alloc_instance` + `Class_init`).

### Virtual Dispatch & Interfaces
- Each class generates `ClassName_VTable` and `static inline ClassName__call_method(...)` dispatchers that evaluate the receiver once with `rt_null_check`.
- `super.method(...)` resolves statically (`SuperClass_method((SuperClass*)this, ...)`).
- Interfaces use **fat pointers** `{ void* obj; const Iface_VTable* vtable; }`. Inherited interfaces are included in `RtTypeInfo` so `(Iface) baseRef` resolves the concrete subclass's vtable via `rt_cast_iface_vtable`.

---

## 4. Generics (Monomorphization) & Concurrency

- **Monomorphization (`Sema.moka`)**: generic classes (`Box<T>`), interfaces (`List<T>`), and methods (`identity<T>(T)`) are cloned and specialized per concrete type argument list on demand (`Box__int`, `Box__String`), skipping uninstantiated templates in `Codegen.moka`.
- **Stop-the-World GC & `RtMonitor`**: `rt_gc_collect()` coordinates all threads via `rt_stw_mutex` / `rt_stw_cond` (`atomic_int rt_stw_requested`), while blocking syscalls (`join`, `sleep`, `Mutex.lock`, `RtMonitor`) mark `in_native_wait = 1` so GC never deadlocks.
