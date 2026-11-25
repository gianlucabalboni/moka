# Milestone 4: Analisi Semantica (`src/sema.h`, `src/sema.c`)

L'analizzatore semantico (`sema_analyze`) opera in **due passaggi** sull'AST e annota i nodi con tutte le informazioni di tipo necessarie a `codegen.c`.

---

## 1. Pass 1: Registrazione delle classi e firme dei membri

- Verifica l'assenza di classi duplicate nell'intero programma (anche tra più file).
- Verifica l'assenza di campi o metodi duplicati dentro ogni classe.
- Risolve tutti i tipi `TY_CLASS` (e ricorsivamente `TY_ARRAY`) in campi, parametri e tipi di ritorno, collegando `resolved_class` al relativo `ClassDecl*`. Questo permette riferimenti circolari e *forward references* tra classi (es. la classe `A` usa `B` definita più avanti).

---

## 2. Pass 2: Type-checking e risoluzione delle espressioni

Mantiene uno stack di scope lessicali (`Scope` con `VarBinding`), la classe/metodo correnti e `loop_depth`:

- **Regola `this.` esplicito**:
  - `this` è vietato nei metodi `static`.
  - L'accesso a campi o metodi d'istanza richiede `this.campo` o `this.metodo(...)`.
  - Nei metodi `static`, una chiamata `foo()` senza prefisso cerca metodi `static` nella classe corrente; altrimenti si usa `ClassName.foo()`.
- **Array & `.length`**:
  - `arr.length` imposta `is_array_length = true` con tipo `int`, ed è marcato come sola lettura (un assegnamento `arr.length = ...` produce errore).
  - L'indicizzazione `arr[i]` richiede un array e un indice numerico (`int` o `byte`).
- **String literal**:
  - Ogni literal `"..."` verifica la presenza della classe `String` nel programma e imposta `inferred_type = String`.
- **Compatibilità di tipi (`expr_assignable`)**:
  - `byte` si amplia automaticamente a `int`.
  - Un literal intero nel range `0..255` è assegnabile a `byte`.
  - `null` è assegnabile a qualsiasi classe o array (`TY_CLASS` / `TY_ARRAY`).
- **Controllo di flusso**:
  - Condizioni di `if` e `while` devono essere `boolean`.
  - `break` e `continue` sono ammessi solo con `loop_depth > 0`.
  - `return` controlla la compatibilità con il `return_type` del metodo (o l'assenza di valore per `void`).

---

## 3. Verifica

I test in [tests/test_sema.c](../tests/test_sema.c) verificano:
1. Riferimenti forward tra classi (`A` -> `B`), `String` literal, `null`, `this.campo`, `.length`.
2. Rifiuto di: `this` in metodi `static`, mismatch di tipi, `break` fuori dal loop, assegnamento a `.length` e chiamata a metodo d'istanza senza `this.`.
