# Ereditarietà e Polimorfismo (`stage1`)

Lo **Stage 1** supporta l'ereditarietà singola (`extends`), l'override dei metodi con **dispatch dinamico (VTable)**, `super(...)` nei costruttori e `super.metodo(...)` / `super.campo`.

---

## 1. Layout delle struct in C11

Le struct vengono emesse in ordine di profondità di ereditarietà:
- Una classe base inizia con `Obj header;`.
- Una sottoclasse (`class Dog extends Animal`) inizia con `Animal base;` come primo campo.
- Poiché `base` è all'offset `0`, ogni `Dog*` è compatibile in memoria con `Animal*` ed `Entity*`, e il GC legge tutti i campi riferimento ereditati tramite `offsetof(OwnerClass, field)`.

---

## 2. Costruttori (`Class_init` + `Class_new`)

- `Class_new(...)` alloca l'istanza con `rt_alloc_instance(..., &Class_type_info)` e chiama `Class_init(this, ...)`.
- `super(args...)` dentro un costruttore chiama `SuperClass_init((SuperClass*)this, args...)`.
- Se un costruttore non chiama esplicitamente `super(...)`, `Codegen.moka` inserisce automaticamente `SuperClass_init((SuperClass*)this);` (richiedendo in `Sema.moka` una chiamata esplicita se il costruttore padre ha parametri).

---

## 3. VTable e Polimorfismo

- `RtTypeInfo` in [stage1/runtime/runtime.h](../runtime/runtime.h) contiene `const void* vtable;`.
- Ogni classe con metodi d'istanza genera `ClassName_VTable`, la relativa istanza `static const ClassName_vtable` e helper `static inline ClassName__call_method(...)` per valutare il ricevitore una sola volta con `rt_null_check`.
- Le chiamate `super.method(...)` vengono risolte staticamente in modo non-virtuale (`SuperClass_method((SuperClass*)this, ...)`).

---

## 4. Verifica

[tests/test_inheritance.moka](../tests/test_inheritance.moka) verifica:
1. Ereditarietà multi-livello (`Entity` → `Animal` → `Dog` / `Cat`).
2. `super(...)` e `super.speak()`.
3. Polimorfismo su `Animal[]` con override di `speak()` e `describe()`.
4. Sopravvivenza dei campi riferimento ereditati (`Entity.name`) attraverso raccolte del Garbage Collector (verificato anche con AddressSanitizer).
