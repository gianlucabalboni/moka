# Milestone 3: AST & Parser (`src/ast.h`, `src/parser.h`, `src/parser.c`)

Il parser trasforma il flusso di `Token` prodotto dal lexer in un **Abstract Syntax Tree (AST)** allocato interamente nell'`RtArena`.

---

## 1. Struttura dell'AST (`src/ast.h`)

Tutti i nodi sono allocati tramite `rt_arena_alloc`, quindi sono già azzerati e vengono liberati in un colpo solo a fine compilazione:

- **`Type`**: rappresenta `void`, `int`, `byte`, `boolean`, `TY_CLASS` (con `class_name` e `resolved_class` per l'analisi semantica), `TY_ARRAY` (puntatore ricorsivo a `elem`) e `TY_NULL`.
- **`Expr`**: include literal (`int`, `char`, `boolean`, `String`, `null`), `this`, identificatori, operatori unari (`-`, `!`) e binari, assegnamento (`=`), accesso a campo (`.`), indicizzazione array (`[i]`), chiamate a metodo (`(...)`), `new Class(...)` e `new T[size]`. Ogni `Expr` ha il campo `inferred_type` che verrà popolato da `sema.c`.
- **`Stmt`**: blocchi `{ ... }`, dichiarazioni di variabili locali (`STMT_VAR_DECL`), `if`/`else`, `while`, `break`, `continue`, `return`, espressioni-statement.
- **`ClassDecl`, `FieldDecl`, `MethodDecl`, `Param`, `Program`**: rappresentano classi, campi, costruttori (massimo uno per classe in Stage 0), metodi normali e metodi `static native`.

---

## 2. Lookahead e disambiguazione (`src/parser.c`)

Il parser è un **Recursive Descent Parser** con un buffer `lookahead[3]`:

- **Dichiarazione di variabile vs Statement (`is_var_decl_start`)**:
  - Se il token corrente è `int`, `byte` o `boolean`, è certamente una dichiarazione di variabile.
  - Se inizia con un identificatore `Foo`:
    - `Foo bar ...` (`lookahead[1] == TOK_IDENT`) → dichiarazione di variabile.
    - `Foo[] bar ...` (`lookahead[1] == '[' && lookahead[2] == ']'`) → dichiarazione di array.
    - Altrimenti (es. `Foo.bar();` o `x = 1;` o `arr[0] = 1;`) → expression statement.
- **Membri di classe**:
  - I modificatori `public` e `private` vengono accettati e ignorati in Stage 0.
  - Se il nome coincide con quello della classe ed è seguito da `(`, viene parsato come **costruttore**.
  - Altrimenti, dopo `Type name`, la presenza di `(` distingue un **metodo** da un **campo** (`;`).
  - I metodi `native` devono essere `static` e terminano con `;` senza corpo.

---

## 3. Precedenza degli operatori

Dal livello più basso al più alto:
1. Assegnamento (`=`, associativo a destra)
2. `||` → `&&` → `==`, `!=` → `<`, `<=`, `>`, `>=`
3. `+`, `-` → `*`, `/`, `%`
4. Unari (`-`, `!`)
5. Postfissi (`.campo`, `[indice]`, `(argomenti)`)
6. Primari (literal, `this`, `null`, identificatori, `(expr)`, `new`)

---

## 4. Verifica

I test in [tests/test_parser.c](../tests/test_parser.c) verificano:
1. Parsing completo di due classi (`String` e `Main`) con campi, costruttore, metodi `static native`, array, `while`/`if`/`break`, escape nelle stringhe.
2. Segnalazione di errori sintattici con file, riga e colonna (es. inizializzazione inline su un campo di classe non ammessa in Stage 0).
