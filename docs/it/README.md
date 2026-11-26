# Documentazione Moka (Italiano)

> 🌐 **Lingua**: [English](../en/README.md) | **Italiano**

Benvenuto nella documentazione ufficiale del linguaggio di programmazione **Moka** (Stage 1).

## Indice

1. **[Guida al Linguaggio](language-guide.md)**
   - Tipi primitivi (`int`, `long`, `float`, `double`, `byte`, `char`, `boolean`), array e `default(T)`
   - Operatori, controllo di flusso (`if`, `for`, `while`, `switch`) e `import`
   - Classi, `extends`, `super`, `interface`, `abstract`, `final`, `static class`, `instanceof`, overloading
   - Eccezioni (`try / catch / finally / throw`)
   - Generici tramite Monomorfizzazione (`Box<T>`, `new Box<>()`, metodi generici)
   - Concorrenza (`Thread`, `Runnable`, `Mutex`, `synchronized`)

2. **[Libreria Standard](stdlib.md)**
   - Classi core caricate automaticamente: `String`, `Console`, `Io`, `ByteBuffer`, `Exception`, `Runnable`, `Thread`, `Mutex`
   - Moduli su richiesta: `std/Math` (`static class Math`)
   - Strutture dati generiche (`std/datastructures/*`): `List<T>`, `ArrayList<T>`, `Stack<T>`, `Queue<T>`, `Map<K, V>`, `StringMap<V>`

3. **[CLI, Installazione e Runtime](cli-and-runtime.md)**
   - Compilazione di Stage 0 e Stage 1
   - Installazione globale (`make install`) e rimozione (`make uninstall`)
   - Opzioni di `mokac` (`-o`, `-r`/`--run`, `--emit-c`, `--keep-c`, `--no-stdlib`, `--std-dir`, `--runtime-dir`)
   - Garbage Collector Stop-the-World Mark-and-Sweep, Shadow Stack, Eccezioni e `RtMonitor`

4. **[Architettura Interna del Compilatore (Stage 0 & Stage 1)](internals.md)**
   - Pipeline di bootstrap self-hosting in due stadi
   - Header `Obj` e `RtTypeInfo`, Shadow Stack, VTable e Fat Pointer delle interfacce, Monomorfizzazione
