/**
 * parser.h — Stage 0 recursive-descent parser interface (3-token lookahead).
 *
 * Parses the Stage 0 Moka subset into an arena-allocated AST Program.
 */
#ifndef MOKA_PARSER_H
#define MOKA_PARSER_H

#include "ast.h"
#include "lexer.h"
#include "runtime.h"
#include <stdbool.h>

typedef struct {
    Lexer lex;
    RtArena* arena;
    Token lookahead[3];
    bool had_error;
} Parser;

void parser_init(Parser* p, RtArena* arena, const char* filename, const char* src, int32_t len);
bool parser_parse_classes(Parser* p, ClassDecl*** classes_out, int32_t* count_out);

#endif /* MOKA_PARSER_H */
