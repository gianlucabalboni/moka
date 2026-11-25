#include "../src/parser.h"
#include "../src/sema.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool run_sema(const char* src) {
    RtArena arena;
    rt_arena_init(&arena);

    Parser p;
    parser_init(&p, &arena, "test.moka", src, (int32_t)strlen(src));
    Program prog;
    bool parsed = parser_parse_classes(&p, &prog.classes, &prog.class_count);
    assert(parsed);

    bool ok = sema_analyze(&prog, &arena);
    rt_arena_free(&arena);
    return ok;
}

int main(void) {
    /* 1. Valid program with forward class reference & String literal */
    const char* valid =
        "class String {\n"
        "    byte[] data;\n"
        "    String(byte[] data) { this.data = data; }\n"
        "    int length() { return this.data.length; }\n"
        "}\n"
        "class A {\n"
        "    B b;\n"
        "    A(B b) { this.b = b; }\n"
        "    int getVal() { return this.b.val; }\n"
        "}\n"
        "class B {\n"
        "    int val;\n"
        "    B(int v) { this.val = v; }\n"
        "}\n"
        "class Main {\n"
        "    static int main() {\n"
        "        B b = new B(42);\n"
        "        A a = new A(b);\n"
        "        A empty = null;\n"
        "        String s = \"ok\";\n"
        "        if (empty == null && s.length() == 2) {\n"
        "            return a.getVal();\n"
        "        }\n"
        "        return 0;\n"
        "    }\n"
        "}\n";
    assert(run_sema(valid));

    /* 2. 'this' inside static method */
    assert(!run_sema("class M { int x; static int f() { return this.x; } }"));

    /* 3. Type mismatch */
    assert(!run_sema("class M { static int f() { int x = true; return x; } }"));

    /* 4. 'break' outside loop */
    assert(!run_sema("class M { static void f() { break; } }"));

    /* 5. Assigning to arr.length */
    assert(!run_sema("class M { static void f() { int[] a = new int[3]; a.length = 5; } }"));

    /* 6. Missing 'this.' on instance method call */
    assert(!run_sema("class M { void g() {} void f() { g(); } }"));

    printf("All sema tests passed!\n");
    return 0;
}
