#include "../src/codegen.h"
#include "../src/parser.h"
#include "../src/sema.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char* src =
        "public class String {\n"
        "    private byte[] data;\n"
        "    public String(byte[] data) { this.data = data; }\n"
        "    public int length() { return this.data.length; }\n"
        "    public byte byteAt(int i) { return this.data[i]; }\n"
        "    public byte[] bytes() { return this.data; }\n"
        "}\n"
        "class Main {\n"
        "    static native void rt_print_int(int x);\n"
        "    static native void rt_println_bytes(byte[] b);\n"
        "    static int fib(int n) {\n"
        "        if (n <= 1) { return n; }\n"
        "        return Main.fib(n - 1) + fib(n - 2);\n"
        "    }\n"
        "    public static int main() {\n"
        "        String msg = \"Hello Moka!\";\n"
        "        Main.rt_println_bytes(msg.bytes());\n"
        "        int f = Main.fib(10);\n"
        "        Main.rt_print_int(f);\n"
        "        int[] arr = new int[4];\n"
        "        int i = 0;\n"
        "        while (i < arr.length) {\n"
        "            arr[i] = i * 2;\n"
        "            i = i + 1;\n"
        "        }\n"
        "        if (arr[3] == 6 && msg.byteAt(0) == 'H') {\n"
        "            return 0;\n"
        "        }\n"
        "        return 1;\n"
        "    }\n"
        "}\n";

    RtArena arena;
    rt_arena_init(&arena);

    Parser p;
    parser_init(&p, &arena, "test.moka", src, (int32_t)strlen(src));
    Program prog;
    assert(parser_parse_classes(&p, &prog.classes, &prog.class_count));
    assert(sema_analyze(&prog, &arena));

    FILE* out = fopen("build/gen_test.c", "w");
    assert(out != NULL);
    assert(codegen_emit(&prog, out, "runtime.h"));
    fclose(out);

    rt_arena_free(&arena);

    int cc_rc = system("cc -std=c11 -Wall -Wextra -Werror -Isrc build/gen_test.c -o build/gen_test");
    assert(cc_rc == 0);

    int run_rc = system("./build/gen_test");
    assert(run_rc == 0);

    printf("All codegen tests passed!\n");
    return 0;
}
