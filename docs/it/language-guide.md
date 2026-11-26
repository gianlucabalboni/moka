# Guida al Linguaggio Moka

> 🌐 **Lingua**: [English](../en/language-guide.md) | **Italiano**

## 1. Punto di Ingresso (`Main`)

Ogni programma Moka definisce una classe `Main` con `public static int main()` (oppure `void main()`):

```moka
class Main {
    public static int main() {
        Console.println("Ciao, Moka!");
        return 0;
    }
}
```

Gli argomenti da riga di comando sono accessibili tramite `Io.args()` (restituisce `String[]`), `Io.argc()` e `Io.argv(int index)`.

---

## 2. Tipi, Letterali e `default(T)`

| Tipo Moka | Rappresentazione C11 | Esempi di Letterali |
|---|---|---|
| `byte` | `uint8_t` | `(byte) 255` |
| `char` | `uint16_t` | `'A'`, `'\n'` |
| `int` | `int32_t` | `42`, `-10` |
| `long` | `int64_t` | `10000000000L` |
| `float` | `float` | `2.5f`, `10f` |
| `double` | `double` | `3.14`, `1.0d` |
| `boolean` | `bool` | `true`, `false` |
| `T[]` | `RtArray*` | `new int[10]`, `arr.length` |
| `Class` / `Interface` | `Cls*` / `{ void*, VTable* }` | `new Box<int>(1)`, `null` |

### Promozioni Numeriche e Cast
- Widening implicito: `byte`/`char` -> `int` -> `long` -> `float` -> `double`.
- Cast espliciti di restringimento: `(int) 3.9`, `(byte) n`, `(char) 65`, `(float) d`.

### `default(T)`
`default(T)` restituisce il valore zero/nullo per qualsiasi tipo `T` (inclusi i parametri di tipo generici):
- `default(int)` -> `0`, `default(double)` -> `0.0`, `default(boolean)` -> `false`, `default(String)` -> `null`.

---

## 3. Operatori e Controllo di Flusso

- **Aritmetici**: `+`, `-`, `*`, `/`, `%` (solo integrali)
- **Incremento / Composti**: `++`, `--`, `+=`, `-=`, `*=`, `/=`, `%=`
- **Bitwise / Shift**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Confronto e Logici**: `==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`
- **Controllo di flusso**: `if / else`, `while`, `for (init; cond; step)`, `switch (expr) { case X: ... default: ... }`, `break`, `continue`, `return`

---

## 4. Stringhe

- `+` e `+=` concatenano `String` con qualsiasi primitivo, `null` o oggetto.
- `concat(String)` restituisce una **nuova** `String` (immutabile, stile Java).
- `append(String)` modifica `this` sul posto e restituisce `this`.
- `toInt()`, `toLong()`, `toDouble()`, `toFloat()` effettuano il parsing e lanciano `Exception` su input non validi.

```moka
String s = "Ciao";
s.append(" ").append("Moka");      // modifica s -> "Ciao Moka"
String msg = s + " #" + 1;         // nuova String -> "Ciao Moka #1"
int n = "42".toInt();              // 42
```

---

## 5. Programmazione Orientata agli Oggetti

- **`this` implicito**: dentro metodi d'istanza e costruttori, campi e metodi della classe corrente (e delle superclassi) possono essere usati senza `this.`. Usa `this.campo` quando un parametro ne oscura il nome.
- **Ereditarietà e Polimorfismo**: ereditarietà singola con `extends`, `super(...)`, `super.metodo(...)`, dispatch virtuale tramite VTable.
- **Interfacce**: `interface` e `implements A, B` (fat pointer `{ obj, vtable }`).
- **`abstract`**: classi astratte e metodi astratti (da implementare nelle sottoclassi concrete).
- **`final`**:
  - `final class`: non estensibile.
  - metodo `final`: non sovrascrivibile (`override` vietato).
  - campo / variabile locale `final`: non riassegnabile dopo l'inizializzazione (supportati anche gli inizializzatori inline come `public static final double PI = 3.14;`).
- **`static class`**: non istanziabile e non estensibile; può contenere solo membri `static` (es. `public static class Math`).
- **Overloading**: metodi e costruttori possono essere sovraccaricati per numero e tipo di parametri.
- **RTTI**: `obj instanceof Tipo` e downcast controllati `(Sottoclasse) ref` / `(MiaInterfaccia) obj`.

---

## 6. Eccezioni

```moka
class MiaException extends Exception {
    public MiaException(String msg) {
        super(msg);
    }
}

try {
    int n = "abc".toInt();
} catch (MiaException me) {
    Console.println(me.getMessage());
} catch (Exception e) {
    Console.println("Errore: " + e.getMessage());
} finally {
    Console.println("Pulizia");
}
```

I blocchi `finally` vengono eseguiti sempre: all'uscita normale, su `return` e su `throw`.

---

## 7. Generici (Monomorfizzazione)

Moka compila i generici tramite **monomorfizzazione**: ogni istanziazione concreta (es. `Box<int>`, `Box<String>`) genera un tipo e funzioni C11 dedicate, permettendo di usare direttamente i tipi primitivi senza boxing.

```moka
interface Container<T> {
    T get();
}

class Box<T> implements Container<T> {
    private T value;
    public Box(T value) { this.value = value; }
    public T get() { return value; }
}

class Util {
    // Sintassi semplificata
    public static T identity<T>(T x) { return x; }
    // Sintassi compatibile con Java
    public static <T> T first(T a, T b) { return a; }
}

Box<int> b1 = new Box<int>(42);
Box<String> b2 = new Box<>("ciao"); // diamond <>
Box<double> b3 = new Box(3.5);      // inferito dall'argomento
int x = Util.identity(10);          // inferito
String s = Util.identity<String>("ok");
```

---

## 8. Concorrenza

- Implementa `Runnable` (`void run()`) oppure estendi `Thread`.
- Avvia con `t.start()`, attendi con `t.join()`, sospendi con `Thread.sleep(ms)`.
- Proteggi lo stato condiviso con `synchronized (obj) { ... }`, metodi d'istanza `synchronized` oppure `Mutex` (`lock()` / `unlock()`).

```moka
class Counter {
    public int value;
    public synchronized void inc() {
        value = value + 1;
    }
}
```

---

## 9. Import

- File locali: `import "helper.moka";`
- Moduli della libreria standard: `import std/Math;` (o `import Math;`), `import std/datastructures/ArrayList;` (o `import datastructures/ArrayList;`).
