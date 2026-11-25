#include "../src/parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_valid_program(void) {
    const char* src =
        "public class String {\n"
        "    private byte[] data;\n"
        "    public String(byte[] data) {\n"
        "        this.data = data;\n"
        "    }\n"
        "    public int length() {\n"
        "        return this.data.length;\n"
        "    }\n"
        "    public byte byteAt(int i) {\n"
        "        return this.data[i];\n"
        "    }\n"
        "}\n"
        "class Main {\n"
        "    static native void rt_print_int(int x);\n"
        "    public static int main() {\n"
        "        String s = \"hello\\n\";\n"
        "        byte[] buf = new byte[10];\n"
        "        String[] list = new String[2];\n"
        "        int i = 0;\n"
        "        while (i < 10) {\n"
        "            if (i == 5) {\n"
        "                break;\n"
        "            }\n"
        "            i = i + 1;\n"
        "        }\n"
        "        Main.rt_print_int(s.length());\n"
        "        return 0;\n"
        "    }\n"
        "}\n";

    RtArena arena;
    rt_arena_init(&arena);

    Parser p;
    parser_init(&p, &arena, "test.moka", src, (int32_t)strlen(src));
    ClassDecl** classes = NULL;
    int32_t count = 0;
    bool ok = parser_parse_classes(&p, &classes, &count);
    assert(ok);
    assert(count == 2);

    /* Check String class */
    assert(strcmp(classes[0]->name, "String") == 0);
    assert(classes[0]->field_count == 1);
    assert(strcmp(classes[0]->fields[0].name, "data") == 0);
    assert(classes[0]->fields[0].type->kind == TY_ARRAY);
    assert(classes[0]->fields[0].type->elem->kind == TY_BYTE);
    assert(classes[0]->constructor != NULL);
    assert(classes[0]->constructor->is_constructor);
    assert(classes[0]->constructor->param_count == 1);
    assert(classes[0]->method_count == 2);

    /* Check Main class */
    assert(strcmp(classes[1]->name, "Main") == 0);
    assert(classes[1]->method_count == 2);
    assert(classes[1]->methods[0]->is_native);
    assert(classes[1]->methods[0]->is_static);
    assert(classes[1]->methods[0]->body == NULL);
    assert(classes[1]->methods[1]->is_static);
    assert(classes[1]->methods[1]->body->kind == STMT_BLOCK);
    assert(classes[1]->methods[1]->body->as.block.count == 7);

    rt_arena_free(&arena);
}

static void test_syntax_error(void) {
    const char* bad = "class Broken { int x = 5; }";
    RtArena arena;
    rt_arena_init(&arena);

    Parser p;
    parser_init(&p, &arena, "bad.moka", bad, (int32_t)strlen(bad));
    ClassDecl** classes = NULL;
    int32_t count = 0;
    bool ok = parser_parse_classes(&p, &classes, &count);
    assert(!ok);

    rt_arena_free(&arena);
}

int main(void) {
    test_valid_program();
    test_syntax_error();
    printf("All parser tests passed!\n");
    return 0;
}
