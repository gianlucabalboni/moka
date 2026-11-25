# Milestone 2: Lexer (`src/lexer.h`, `src/lexer.c`)

Il lexer converte il buffer sorgente `.moka` in un flusso di `Token` senza allocare memoria sullo heap (approccio **zero-copy**).

---

## 1. Rappresentazione dei Token (`Token`)

Ogni token definito in [src/lexer.h](../src/lexer.h) contiene:
- **`kind`** (`TokenKind`): tipo del token (keyword, identificatore, literal, operatore, delimitatore, `TOK_EOF`, `TOK_ERROR`).
- **`start` e `length`**: puntatore diretto nel buffer sorgente originale e lunghezza in byte. Non vengono copiate stringhe durante la tokenizzazione.
- **`line` e `col`**: posizione esatta nel sorgente (1-based) per produrre messaggi di errore chiari nel parser e nell'analisi semantica.

---

## 2. Gestione degli errori senza eccezioni

Quando il lexer incontra un errore lessicale (es. stringa o commento `/* ... */` non terminato, sequenza di escape invalida, singolo `&` o `|`), restituisce un token con `kind = TOK_ERROR` in cui `start` punta a un messaggio statico in inglese e `line`/`col` indicano la posizione dell'errore.

---

## 3. Regole lessicali supportate

- **Commenti**: `// ...` fino a fine riga e `/* ... */` multilinea (con controllo di chiusura prima di `EOF`).
- **Keyword**: `class`, `public`, `private`, `static`, `native`, `void`, `int`, `byte`, `boolean`, `true`, `false`, `null`, `this`, `new`, `if`, `else`, `while`, `break`, `continue`, `return`.
- **Literal**:
  - Interi (`TOK_INT_LIT`): sequenze di cifre `0-9`.
  - Caratteri (`TOK_CHAR_LIT`): `'a'` o sequenze di escape `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\'`.
  - Stringhe (`TOK_STRING_LIT`): `"..."` su singola riga con le stesse sequenze di escape.
- **Operatori e delimitatori**: tutti gli operatori aritmetici, logici (`&&`, `||`, `!`), relazionali (`==`, `!=`, `<`, `<=`, `>`, `>=`), assegnamento (`=`) e punteggiatura (`( ) { } [ ] ; , .`).

---

## 4. Verifica

I test in [tests/test_lexer.c](../tests/test_lexer.c) verificano:
1. Tokenizzazione completa di un programma con commenti `//` e `/* */`, char/string literal con escape e operatori.
2. Tracciamento corretto di `line` e confronto `token_equals`.
3. Rilevamento errori su stringhe non chiuse, commenti non chiusi, char literal vuoti/multi-carattere e `&` isolato.
