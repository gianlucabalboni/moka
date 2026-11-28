/**
 * lexer.h — Stage 0 zero-copy tokenizer interface.
 *
 * Defines TokenKind, Token (slice into source buffer with line/col), and
 * Lexer API for scanning the Stage 0 Moka subset.
 */
#ifndef MOKA_LEXER_H
#define MOKA_LEXER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TOK_EOF = 0,
    TOK_ERROR,

    /* Identifiers and literals */
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_CHAR_LIT,
    TOK_STRING_LIT,

    /* Keywords */
    TOK_KW_CLASS,
    TOK_KW_PUBLIC,
    TOK_KW_PRIVATE,
    TOK_KW_STATIC,
    TOK_KW_NATIVE,
    TOK_KW_VOID,
    TOK_KW_INT,
    TOK_KW_BYTE,
    TOK_KW_BOOLEAN,
    TOK_KW_TRUE,
    TOK_KW_FALSE,
    TOK_KW_NULL,
    TOK_KW_THIS,
    TOK_KW_NEW,
    TOK_KW_IF,
    TOK_KW_ELSE,
    TOK_KW_WHILE,
    TOK_KW_BREAK,
    TOK_KW_CONTINUE,
    TOK_KW_RETURN,

    /* Delimiters */
    TOK_LPAREN,    /* ( */
    TOK_RPAREN,    /* ) */
    TOK_LBRACE,    /* { */
    TOK_RBRACE,    /* } */
    TOK_LBRACKET,  /* [ */
    TOK_RBRACKET,  /* ] */
    TOK_SEMI,      /* ; */
    TOK_COMMA,     /* , */
    TOK_DOT,       /* . */

    /* Operators */
    TOK_ASSIGN,    /* = */
    TOK_PLUS,      /* + */
    TOK_MINUS,     /* - */
    TOK_STAR,      /* * */
    TOK_SLASH,     /* / */
    TOK_PERCENT,   /* % */
    TOK_BANG,      /* ! */
    TOK_EQ,        /* == */
    TOK_NEQ,       /* != */
    TOK_LT,        /* < */
    TOK_LE,        /* <= */
    TOK_GT,        /* > */
    TOK_GE,        /* >= */
    TOK_AND,       /* && */
    TOK_OR         /* || */
} TokenKind;

typedef struct {
    TokenKind kind;
    const char* start;
    int32_t length;
    int32_t line;
    int32_t col;
} Token;

typedef struct {
    const char* filename;
    const char* src;
    int32_t len;
    int32_t pos;
    int32_t line;
    int32_t col;
} Lexer;

void lexer_init(Lexer* lex, const char* filename, const char* src, int32_t len);
Token lexer_next(Lexer* lex);
const char* token_kind_name(TokenKind kind);
bool token_equals(Token tok, const char* cstr);

#endif /* MOKA_LEXER_H */
