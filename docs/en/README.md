# Moka Documentation (English)

> 🌐 **Language**: **English** | [Italiano](../it/README.md)

Welcome to the official documentation for the **Moka** programming language (Stage 1).

## Contents

1. **[Language Guide](language-guide.md)**
   - Primitive types (`int`, `long`, `float`, `double`, `byte`, `char`, `boolean`), arrays, and `default(T)`
   - Operators, control flow (`if`, `for`, `while`, `switch`), and `import`
   - Classes, `extends`, `super`, `interface`, `abstract`, `final`, `static class`, `instanceof`, overloading
   - Exceptions (`try / catch / finally / throw`)
   - Generics via Monomorphization (`Box<T>`, `new Box<>()`, generic methods)
   - Concurrency (`Thread`, `Runnable`, `Mutex`, `synchronized`)

2. **[Standard Library Reference](stdlib.md)**
   - Core auto-loaded classes: `String`, `Console`, `Io`, `ByteBuffer`, `Exception`, `Runnable`, `Thread`, `Mutex`
   - On-demand modules: `std/Math` (`static class Math`)
   - Generic data structures (`std/datastructures/*`): `List<T>`, `ArrayList<T>`, `Stack<T>`, `Queue<T>`, `Map<K, V>`, `StringMap<V>`

3. **[CLI, Installation & Runtime](cli-and-runtime.md)**
   - Building Stage 0 and Stage 1
   - Global installation (`make install`) and removal (`make uninstall`)
   - `mokac` flags (`-o`, `-r`/`--run`, `--emit-c`, `--keep-c`, `--no-stdlib`, `--std-dir`, `--runtime-dir`)
   - Stop-the-World Mark-and-Sweep GC, Shadow Stack, Exceptions, and `RtMonitor`

4. **[Compiler Internals (Stage 0 & Stage 1)](internals.md)**
   - Two-stage self-hosting bootstrap pipeline
   - Object header & `RtTypeInfo`, Shadow Stack, VTables & Interface Fat Pointers, Monomorphization
