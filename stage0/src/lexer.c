#include "lexer.h"
#include <string.h>

typedef struct {
    const char* text;
    TokenKind kind;
} KeywordEntry;

static const KeywordEntry KEYWORDS[] = {
    {"class",    TOK_KW_CLASS},
    {"public",   TOK_KW_PUBLIC},
    {"private",  TOK_KW_PRIVATE},
    {"static",   TOK_KW_STATIC},
    {"native",   TOK_KW_NATIVE},
    {"void",     TOK_KW_VOID},
    {"int",      TOK_KW_INT},
    {"byte",     TOK_KW_BYTE},
    {"boolean",  TOK_KW_BOOLEAN},
    {"true",     TOK_KW_TRUE},
    {"false",    TOK_KW_FALSE},
    {"null",     TOK_KW_NULL},
    {"this",     TOK_KW_THIS},
    {"new",      TOK_KW_NEW},
    {"if",       TOK_KW_IF},
    {"else",     TOK_KW_ELSE},
    {"while",    TOK_KW_WHILE},
    {"break",    TOK_KW_BREAK},
    {"continue", TOK_KW_CONTINUE},
    {"return",   TOK_KW_RETURN},
    {NULL,       TOK_EOF}
};

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static bool is_at_end(const Lexer* lex) {
    return lex->pos >= lex->len;
}

static char peek(const Lexer* lex) {
    if (is_at_end(lex)) {
        return '\0';
    }
    return lex->src[lex->pos];
}

static char peek_next(const Lexer* lex) {
    if (lex->pos + 1 >= lex->len) {
        return '\0';
    }
    return lex->src[lex->pos + 1];
}

static char advance(Lexer* lex) {
    char c = lex->src[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

static Token make_token(TokenKind kind, const char* start, int32_t length,
                        int32_t line, int32_t col) {
    Token tok;
    tok.kind = kind;
    tok.start = start;
    tok.length = length;
    tok.line = line;
    tok.col = col;
    return tok;
}

static Token error_token(const char* message, int32_t line, int32_t col) {
    return make_token(TOK_ERROR, message, (int32_t)strlen(message), line, col);
}

static bool skip_whitespace_and_comments(Lexer* lex, Token* err_out) {
    for (;;) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
        } else if (c == '/' && peek_next(lex) == '/') {
            while (!is_at_end(lex) && peek(lex) != '\n') {
                advance(lex);
            }
        } else if (c == '/' && peek_next(lex) == '*') {
            int32_t start_line = lex->line;
            int32_t start_col = lex->col;
            advance(lex); /* '/' */
            advance(lex); /* '*' */
            bool closed = false;
            while (!is_at_end(lex)) {
                if (peek(lex) == '*' && peek_next(lex) == '/') {
                    advance(lex);
                    advance(lex);
                    closed = true;
                    break;
                }
                advance(lex);
            }
            if (!closed) {
                *err_out = error_token("unterminated block comment", start_line, start_col);
                return false;
            }
        } else {
            return true;
        }
    }
}

static TokenKind lookup_keyword(const char* start, int32_t length) {
    for (int i = 0; KEYWORDS[i].text != NULL; i++) {
        size_t kw_len = strlen(KEYWORDS[i].text);
        if ((int32_t)kw_len == length && memcmp(start, KEYWORDS[i].text, kw_len) == 0) {
            return KEYWORDS[i].kind;
        }
    }
    return TOK_IDENT;
}

static Token lex_identifier(Lexer* lex, int32_t start_pos, int32_t line, int32_t col) {
    while (!is_at_end(lex) && is_alnum(peek(lex))) {
        advance(lex);
    }
    const char* start = lex->src + start_pos;
    int32_t length = lex->pos - start_pos;
    TokenKind kind = lookup_keyword(start, length);
    return make_token(kind, start, length, line, col);
}

static Token lex_number(Lexer* lex, int32_t start_pos, int32_t line, int32_t col) {
    while (!is_at_end(lex) && is_digit(peek(lex))) {
        advance(lex);
    }
    return make_token(TOK_INT_LIT, lex->src + start_pos, lex->pos - start_pos, line, col);
}

static bool is_valid_escape(char c) {
    return c == 'n' || c == 't' || c == 'r' || c == '0' ||
           c == '\\' || c == '"' || c == '\'';
}

static Token lex_string(Lexer* lex, int32_t start_pos, int32_t line, int32_t col) {
    while (!is_at_end(lex)) {
        char c = peek(lex);
        if (c == '"') {
            advance(lex);
            return make_token(TOK_STRING_LIT, lex->src + start_pos,
                              lex->pos - start_pos, line, col);
        }
        if (c == '\n') {
            return error_token("unterminated string literal", line, col);
        }
        if (c == '\\') {
            advance(lex);
            if (is_at_end(lex) || !is_valid_escape(peek(lex))) {
                return error_token("invalid escape sequence in string literal",
                                   lex->line, lex->col);
            }
        }
        advance(lex);
    }
    return error_token("unterminated string literal", line, col);
}

static Token lex_char(Lexer* lex, int32_t start_pos, int32_t line, int32_t col) {
    if (is_at_end(lex) || peek(lex) == '\n') {
        return error_token("unterminated character literal", line, col);
    }
    if (peek(lex) == '\'') {
        return error_token("empty character literal", line, col);
    }
    char c = advance(lex);
    if (c == '\\') {
        if (is_at_end(lex) || !is_valid_escape(peek(lex))) {
            return error_token("invalid escape sequence in character literal",
                               lex->line, lex->col);
        }
        advance(lex);
    }
    if (is_at_end(lex) || peek(lex) != '\'') {
        return error_token("unterminated character literal", line, col);
    }
    advance(lex); /* closing '\'' */
    return make_token(TOK_CHAR_LIT, lex->src + start_pos, lex->pos - start_pos, line, col);
}

void lexer_init(Lexer* lex, const char* filename, const char* src, int32_t len) {
    lex->filename = filename;
    lex->src = src;
    lex->len = len;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
}

Token lexer_next(Lexer* lex) {
    Token err;
    if (!skip_whitespace_and_comments(lex, &err)) {
        return err;
    }

    if (is_at_end(lex)) {
        return make_token(TOK_EOF, lex->src + lex->pos, 0, lex->line, lex->col);
    }

    int32_t start_pos = lex->pos;
    int32_t line = lex->line;
    int32_t col = lex->col;
    char c = advance(lex);

    if (is_alpha(c)) {
        return lex_identifier(lex, start_pos, line, col);
    }
    if (is_digit(c)) {
        return lex_number(lex, start_pos, line, col);
    }
    if (c == '"') {
        return lex_string(lex, start_pos, line, col);
    }
    if (c == '\'') {
        return lex_char(lex, start_pos, line, col);
    }

    const char* s = lex->src + start_pos;
    switch (c) {
        case '(': return make_token(TOK_LPAREN, s, 1, line, col);
        case ')': return make_token(TOK_RPAREN, s, 1, line, col);
        case '{': return make_token(TOK_LBRACE, s, 1, line, col);
        case '}': return make_token(TOK_RBRACE, s, 1, line, col);
        case '[': return make_token(TOK_LBRACKET, s, 1, line, col);
        case ']': return make_token(TOK_RBRACKET, s, 1, line, col);
        case ';': return make_token(TOK_SEMI, s, 1, line, col);
        case ',': return make_token(TOK_COMMA, s, 1, line, col);
        case '.': return make_token(TOK_DOT, s, 1, line, col);
        case '+': return make_token(TOK_PLUS, s, 1, line, col);
        case '-': return make_token(TOK_MINUS, s, 1, line, col);
        case '*': return make_token(TOK_STAR, s, 1, line, col);
        case '/': return make_token(TOK_SLASH, s, 1, line, col);
        case '%': return make_token(TOK_PERCENT, s, 1, line, col);
        case '=':
            if (peek(lex) == '=') {
                advance(lex);
                return make_token(TOK_EQ, s, 2, line, col);
            }
            return make_token(TOK_ASSIGN, s, 1, line, col);
        case '!':
            if (peek(lex) == '=') {
                advance(lex);
                return make_token(TOK_NEQ, s, 2, line, col);
            }
            return make_token(TOK_BANG, s, 1, line, col);
        case '<':
            if (peek(lex) == '=') {
                advance(lex);
                return make_token(TOK_LE, s, 2, line, col);
            }
            return make_token(TOK_LT, s, 1, line, col);
        case '>':
            if (peek(lex) == '=') {
                advance(lex);
                return make_token(TOK_GE, s, 2, line, col);
            }
            return make_token(TOK_GT, s, 1, line, col);
        case '&':
            if (peek(lex) == '&') {
                advance(lex);
                return make_token(TOK_AND, s, 2, line, col);
            }
            return error_token("unexpected character '&' (expected '&&')", line, col);
        case '|':
            if (peek(lex) == '|') {
                advance(lex);
                return make_token(TOK_OR, s, 2, line, col);
            }
            return error_token("unexpected character '|' (expected '||')", line, col);
        default:
            return error_token("unexpected character", line, col);
    }
}

const char* token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOK_EOF:         return "EOF";
        case TOK_ERROR:       return "ERROR";
        case TOK_IDENT:       return "IDENT";
        case TOK_INT_LIT:     return "INT_LIT";
        case TOK_CHAR_LIT:    return "CHAR_LIT";
        case TOK_STRING_LIT:  return "STRING_LIT";
        case TOK_KW_CLASS:    return "class";
        case TOK_KW_PUBLIC:   return "public";
        case TOK_KW_PRIVATE:  return "private";
        case TOK_KW_STATIC:   return "static";
        case TOK_KW_NATIVE:   return "native";
        case TOK_KW_VOID:     return "void";
        case TOK_KW_INT:      return "int";
        case TOK_KW_BYTE:     return "byte";
        case TOK_KW_BOOLEAN:  return "boolean";
        case TOK_KW_TRUE:     return "true";
        case TOK_KW_FALSE:    return "false";
        case TOK_KW_NULL:     return "null";
        case TOK_KW_THIS:     return "this";
        case TOK_KW_NEW:      return "new";
        case TOK_KW_IF:       return "if";
        case TOK_KW_ELSE:     return "else";
        case TOK_KW_WHILE:    return "while";
        case TOK_KW_BREAK:    return "break";
        case TOK_KW_CONTINUE: return "continue";
        case TOK_KW_RETURN:   return "return";
        case TOK_LPAREN:      return "(";
        case TOK_RPAREN:      return ")";
        case TOK_LBRACE:      return "{";
        case TOK_RBRACE:      return "}";
        case TOK_LBRACKET:    return "[";
        case TOK_RBRACKET:    return "]";
        case TOK_SEMI:        return ";";
        case TOK_COMMA:       return ",";
        case TOK_DOT:         return ".";
        case TOK_ASSIGN:      return "=";
        case TOK_PLUS:        return "+";
        case TOK_MINUS:       return "-";
        case TOK_STAR:        return "*";
        case TOK_SLASH:       return "/";
        case TOK_PERCENT:     return "%";
        case TOK_BANG:        return "!";
        case TOK_EQ:          return "==";
        case TOK_NEQ:         return "!=";
        case TOK_LT:          return "<";
        case TOK_LE:          return "<=";
        case TOK_GT:          return ">";
        case TOK_GE:          return ">=";
        case TOK_AND:         return "&&";
        case TOK_OR:          return "||";
    }
    return "UNKNOWN";
}

bool token_equals(Token tok, const char* cstr) {
    size_t len = strlen(cstr);
    return (int32_t)len == tok.length && memcmp(tok.start, cstr, len) == 0;
}
