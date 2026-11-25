#include "../src/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_basic_tokens(void) {
    const char* src =
        "// line comment\n"
        "public class Main {\n"
        "    /* block\n"
        "       comment */\n"
        "    static int main() {\n"
        "        byte c = '\\n';\n"
        "        String s = \"hi\\t\\\"moka\\\"\";\n"
        "        int x = 42;\n"
        "        if (x >= 10 && x != 0 || !false) {\n"
        "            return x;\n"
        "        }\n"
        "        return 0;\n"
        "    }\n"
        "}\n";

    Lexer lex;
    lexer_init(&lex, "test.moka", src, (int32_t)strlen(src));

    TokenKind expected[] = {
        TOK_KW_PUBLIC, TOK_KW_CLASS, TOK_IDENT, TOK_LBRACE,
        TOK_KW_STATIC, TOK_KW_INT, TOK_IDENT, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE,
        TOK_KW_BYTE, TOK_IDENT, TOK_ASSIGN, TOK_CHAR_LIT, TOK_SEMI,
        TOK_IDENT, TOK_IDENT, TOK_ASSIGN, TOK_STRING_LIT, TOK_SEMI,
        TOK_KW_INT, TOK_IDENT, TOK_ASSIGN, TOK_INT_LIT, TOK_SEMI,
        TOK_KW_IF, TOK_LPAREN, TOK_IDENT, TOK_GE, TOK_INT_LIT, TOK_AND,
        TOK_IDENT, TOK_NEQ, TOK_INT_LIT, TOK_OR, TOK_BANG, TOK_KW_FALSE,
        TOK_RPAREN, TOK_LBRACE,
        TOK_KW_RETURN, TOK_IDENT, TOK_SEMI,
        TOK_RBRACE,
        TOK_KW_RETURN, TOK_INT_LIT, TOK_SEMI,
        TOK_RBRACE,
        TOK_RBRACE,
        TOK_EOF
    };

    int count = (int)(sizeof(expected) / sizeof(expected[0]));
    for (int i = 0; i < count; i++) {
        Token tok = lexer_next(&lex);
        assert(tok.kind == expected[i]);
        if (i == 2) {
            assert(token_equals(tok, "Main"));
            assert(tok.line == 2);
        }
    }
}

static void test_errors(void) {
    Lexer lex;

    const char* bad_str = "\"unterminated\n";
    lexer_init(&lex, "err.moka", bad_str, (int32_t)strlen(bad_str));
    assert(lexer_next(&lex).kind == TOK_ERROR);

    const char* bad_comment = "/* unterminated";
    lexer_init(&lex, "err.moka", bad_comment, (int32_t)strlen(bad_comment));
    assert(lexer_next(&lex).kind == TOK_ERROR);

    const char* bad_char = "'ab'";
    lexer_init(&lex, "err.moka", bad_char, (int32_t)strlen(bad_char));
    assert(lexer_next(&lex).kind == TOK_ERROR);

    const char* bad_amp = "a & b";
    lexer_init(&lex, "err.moka", bad_amp, (int32_t)strlen(bad_amp));
    assert(lexer_next(&lex).kind == TOK_IDENT);
    assert(lexer_next(&lex).kind == TOK_ERROR);
}

int main(void) {
    test_basic_tokens();
    test_errors();
    printf("All lexer tests passed!\n");
    return 0;
}
