#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Temporary Dynamic Vector Helper (copied into Arena when finalized)
 * ========================================================================= */

typedef struct {
    void** items;
    int32_t count;
    int32_t cap;
} PtrVec;

static void ptr_vec_init(PtrVec* v) {
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static void ptr_vec_push(PtrVec* v, void* item) {
    if (v->count >= v->cap) {
        int32_t new_cap = v->cap == 0 ? 8 : v->cap * 2;
        void** new_items = (void**)realloc(v->items, (size_t)new_cap * sizeof(void*));
        if (!new_items) {
            fprintf(stderr, "moka compiler error: out of memory\n");
            exit(1);
        }
        v->items = new_items;
        v->cap = new_cap;
    }
    v->items[v->count++] = item;
}

static void* ptr_vec_to_arena(PtrVec* v, RtArena* arena, size_t elem_size) {
    if (v->count == 0) {
        free(v->items);
        v->items = NULL;
        return NULL;
    }
    void* buf = rt_arena_alloc(arena, (size_t)v->count * elem_size);
    memcpy(buf, v->items, (size_t)v->count * elem_size);
    free(v->items);
    v->items = NULL;
    return buf;
}

/* =========================================================================
 * Arena & Token Helpers
 * ========================================================================= */

static char* arena_strdup(RtArena* arena, const char* s, int32_t len) {
    char* copy = (char*)rt_arena_alloc(arena, (size_t)len + 1);
    if (len > 0) {
        memcpy(copy, s, (size_t)len);
    }
    copy[len] = '\0';
    return copy;
}

static Token peek(const Parser* p) {
    return p->lookahead[0];
}

static Token peek_at(const Parser* p, int idx) {
    return p->lookahead[idx];
}

static Token advance_tok(Parser* p) {
    Token cur = p->lookahead[0];
    p->lookahead[0] = p->lookahead[1];
    p->lookahead[1] = p->lookahead[2];
    p->lookahead[2] = lexer_next(&p->lex);
    return cur;
}

static void parse_error(Parser* p, Token tok, const char* msg) {
    if (p->had_error) {
        return; /* Report first error cleanly without cascading */
    }
    p->had_error = true;
    if (tok.kind == TOK_ERROR) {
        fprintf(stderr, "%s:%d:%d: lexical error: %.*s\n",
                p->lex.filename, tok.line, tok.col, tok.length, tok.start);
    } else {
        fprintf(stderr, "%s:%d:%d: syntax error: %s (got '%s')\n",
                p->lex.filename, tok.line, tok.col, msg, token_kind_name(tok.kind));
    }
}

static bool match(Parser* p, TokenKind kind) {
    if (peek(p).kind == kind) {
        advance_tok(p);
        return true;
    }
    return false;
}

static Token expect(Parser* p, TokenKind kind, const char* msg) {
    Token tok = peek(p);
    if (tok.kind != kind) {
        parse_error(p, tok, msg);
        return tok;
    }
    return advance_tok(p);
}

static uint8_t decode_escape(char c) {
    switch (c) {
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'r':  return '\r';
        case '0':  return '\0';
        case '\\': return '\\';
        case '"':  return '"';
        case '\'': return '\'';
        default:   return (uint8_t)c;
    }
}

static void decode_string_lit(Parser* p, Token tok, const char** out_bytes, int32_t* out_len) {
    /* tok.start includes opening and closing quotes */
    const char* raw = tok.start + 1;
    int32_t raw_len = tok.length - 2;
    char* buf = (char*)rt_arena_alloc(p->arena, (size_t)raw_len + 1);
    int32_t out = 0;
    for (int32_t i = 0; i < raw_len; i++) {
        if (raw[i] == '\\' && i + 1 < raw_len) {
            i++;
            buf[out++] = (char)decode_escape(raw[i]);
        } else {
            buf[out++] = raw[i];
        }
    }
    buf[out] = '\0';
    *out_bytes = buf;
    *out_len = out;
}

static uint8_t decode_char_lit(Token tok) {
    /* tok.start is '...' */
    if (tok.length >= 4 && tok.start[1] == '\\') {
        return decode_escape(tok.start[2]);
    }
    return (uint8_t)tok.start[1];
}

/* =========================================================================
 * Forward Declarations
 * ========================================================================= */

static Type* parse_type(Parser* p, bool allow_void);
static Stmt* parse_stmt(Parser* p);
static Stmt* parse_block(Parser* p);
static Expr* parse_expr(Parser* p);

/* =========================================================================
 * Types
 * ========================================================================= */

static Type* make_type(RtArena* arena, TypeKind kind) {
    Type* ty = (Type*)rt_arena_alloc(arena, sizeof(Type));
    ty->kind = kind;
    return ty;
}

static Type* parse_base_type(Parser* p, bool allow_void) {
    Token tok = peek(p);
    if (tok.kind == TOK_KW_INT) {
        advance_tok(p);
        return make_type(p->arena, TY_INT);
    }
    if (tok.kind == TOK_KW_BYTE) {
        advance_tok(p);
        return make_type(p->arena, TY_BYTE);
    }
    if (tok.kind == TOK_KW_BOOLEAN) {
        advance_tok(p);
        return make_type(p->arena, TY_BOOLEAN);
    }
    if (tok.kind == TOK_KW_VOID) {
        if (!allow_void) {
            parse_error(p, tok, "'void' is not allowed as a variable or field type");
            return make_type(p->arena, TY_VOID);
        }
        advance_tok(p);
        return make_type(p->arena, TY_VOID);
    }
    if (tok.kind == TOK_IDENT) {
        advance_tok(p);
        Type* ty = make_type(p->arena, TY_CLASS);
        ty->class_name = arena_strdup(p->arena, tok.start, tok.length);
        return ty;
    }
    parse_error(p, tok, "expected type name");
    return make_type(p->arena, TY_INT);
}

static Type* parse_type(Parser* p, bool allow_void) {
    Type* ty = parse_base_type(p, allow_void);
    while (!p->had_error && peek(p).kind == TOK_LBRACKET) {
        if (ty->kind == TY_VOID) {
            parse_error(p, peek(p), "cannot create array of void");
            return ty;
        }
        advance_tok(p); /* '[' */
        expect(p, TOK_RBRACKET, "expected ']' in array type");
        Type* arr = make_type(p->arena, TY_ARRAY);
        arr->elem = ty;
        ty = arr;
    }
    return ty;
}

/* =========================================================================
 * Expressions
 * ========================================================================= */

static Expr* new_expr(Parser* p, ExprKind kind, Token tok) {
    Expr* e = (Expr*)rt_arena_alloc(p->arena, sizeof(Expr));
    e->kind = kind;
    e->line = tok.line;
    e->col = tok.col;
    return e;
}

static void parse_arg_list(Parser* p, Expr*** args_out, int32_t* count_out) {
    PtrVec args;
    ptr_vec_init(&args);
    if (peek(p).kind != TOK_RPAREN) {
        do {
            Expr* arg = parse_expr(p);
            ptr_vec_push(&args, arg);
        } while (!p->had_error && match(p, TOK_COMMA));
    }
    expect(p, TOK_RPAREN, "expected ')' after argument list");
    *count_out = args.count;
    *args_out = (Expr**)ptr_vec_to_arena(&args, p->arena, sizeof(Expr*));
}

static Expr* parse_primary(Parser* p) {
    Token tok = peek(p);
    switch (tok.kind) {
        case TOK_INT_LIT: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_INT_LIT, tok);
            char* tmp = arena_strdup(p->arena, tok.start, tok.length);
            e->as.int_lit.val = (int32_t)strtol(tmp, NULL, 10);
            return e;
        }
        case TOK_CHAR_LIT: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_CHAR_LIT, tok);
            e->as.char_lit.val = decode_char_lit(tok);
            return e;
        }
        case TOK_KW_TRUE: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_BOOL_LIT, tok);
            e->as.bool_lit.val = true;
            return e;
        }
        case TOK_KW_FALSE: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_BOOL_LIT, tok);
            e->as.bool_lit.val = false;
            return e;
        }
        case TOK_STRING_LIT: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_STRING_LIT, tok);
            decode_string_lit(p, tok, &e->as.string_lit.bytes, &e->as.string_lit.len);
            return e;
        }
        case TOK_KW_NULL: {
            advance_tok(p);
            return new_expr(p, EXPR_NULL, tok);
        }
        case TOK_KW_THIS: {
            advance_tok(p);
            return new_expr(p, EXPR_THIS, tok);
        }
        case TOK_IDENT: {
            advance_tok(p);
            Expr* e = new_expr(p, EXPR_IDENT, tok);
            e->as.ident.name = arena_strdup(p->arena, tok.start, tok.length);
            return e;
        }
        case TOK_LPAREN: {
            advance_tok(p);
            Expr* e = parse_expr(p);
            expect(p, TOK_RPAREN, "expected ')' after expression");
            return e;
        }
        case TOK_KW_NEW: {
            advance_tok(p);
            Type* base = parse_base_type(p, false);
            if (p->had_error) {
                return new_expr(p, EXPR_NULL, tok);
            }
            if (peek(p).kind == TOK_LPAREN) {
                if (base->kind != TY_CLASS) {
                    parse_error(p, peek(p), "cannot instantiate primitive type with 'new(...)'");
                    return new_expr(p, EXPR_NULL, tok);
                }
                advance_tok(p); /* '(' */
                Expr* e = new_expr(p, EXPR_NEW_OBJ, tok);
                e->as.new_obj.class_name = base->class_name;
                parse_arg_list(p, &e->as.new_obj.args, &e->as.new_obj.arg_count);
                return e;
            }
            if (peek(p).kind == TOK_LBRACKET) {
                advance_tok(p); /* '[' */
                Expr* size = parse_expr(p);
                expect(p, TOK_RBRACKET, "expected ']' after array size");
                Expr* e = new_expr(p, EXPR_NEW_ARRAY, tok);
                e->as.new_array.elem_type = base;
                e->as.new_array.size = size;
                return e;
            }
            parse_error(p, peek(p), "expected '(' or '[' after type in 'new' expression");
            return new_expr(p, EXPR_NULL, tok);
        }
        default:
            parse_error(p, tok, "expected expression");
            return new_expr(p, EXPR_NULL, tok);
    }
}

static Expr* parse_postfix(Parser* p) {
    Expr* expr = parse_primary(p);
    while (!p->had_error) {
        Token tok = peek(p);
        if (tok.kind == TOK_DOT) {
            advance_tok(p);
            Token name = expect(p, TOK_IDENT, "expected field or method name after '.'");
            Expr* f = new_expr(p, EXPR_FIELD, name);
            f->as.field.object = expr;
            f->as.field.name = arena_strdup(p->arena, name.start, name.length);
            expr = f;
        } else if (tok.kind == TOK_LBRACKET) {
            advance_tok(p);
            Expr* idx = parse_expr(p);
            expect(p, TOK_RBRACKET, "expected ']' after array index");
            Expr* ix = new_expr(p, EXPR_INDEX, tok);
            ix->as.index.array = expr;
            ix->as.index.index = idx;
            expr = ix;
        } else if (tok.kind == TOK_LPAREN) {
            advance_tok(p);
            Expr* call = new_expr(p, EXPR_CALL, tok);
            call->as.call.callee = expr;
            parse_arg_list(p, &call->as.call.args, &call->as.call.arg_count);
            expr = call;
        } else {
            break;
        }
    }
    return expr;
}

static Expr* parse_unary(Parser* p) {
    Token tok = peek(p);
    if (tok.kind == TOK_MINUS || tok.kind == TOK_BANG) {
        advance_tok(p);
        Expr* e = new_expr(p, EXPR_UNARY, tok);
        e->as.unary.op = (tok.kind == TOK_MINUS) ? UNOP_NEG : UNOP_NOT;
        e->as.unary.operand = parse_unary(p);
        return e;
    }
    return parse_postfix(p);
}

static Expr* make_binary(Parser* p, Token tok, BinaryOp op, Expr* left, Expr* right) {
    Expr* e = new_expr(p, EXPR_BINARY, tok);
    e->as.binary.op = op;
    e->as.binary.left = left;
    e->as.binary.right = right;
    return e;
}

static Expr* parse_mul(Parser* p) {
    Expr* left = parse_unary(p);
    while (!p->had_error) {
        Token tok = peek(p);
        BinaryOp op;
        if (tok.kind == TOK_STAR) op = BINOP_MUL;
        else if (tok.kind == TOK_SLASH) op = BINOP_DIV;
        else if (tok.kind == TOK_PERCENT) op = BINOP_MOD;
        else break;
        advance_tok(p);
        left = make_binary(p, tok, op, left, parse_unary(p));
    }
    return left;
}

static Expr* parse_add(Parser* p) {
    Expr* left = parse_mul(p);
    while (!p->had_error) {
        Token tok = peek(p);
        BinaryOp op;
        if (tok.kind == TOK_PLUS) op = BINOP_ADD;
        else if (tok.kind == TOK_MINUS) op = BINOP_SUB;
        else break;
        advance_tok(p);
        left = make_binary(p, tok, op, left, parse_mul(p));
    }
    return left;
}

static Expr* parse_rel(Parser* p) {
    Expr* left = parse_add(p);
    while (!p->had_error) {
        Token tok = peek(p);
        BinaryOp op;
        if (tok.kind == TOK_LT) op = BINOP_LT;
        else if (tok.kind == TOK_LE) op = BINOP_LE;
        else if (tok.kind == TOK_GT) op = BINOP_GT;
        else if (tok.kind == TOK_GE) op = BINOP_GE;
        else break;
        advance_tok(p);
        left = make_binary(p, tok, op, left, parse_add(p));
    }
    return left;
}

static Expr* parse_eq(Parser* p) {
    Expr* left = parse_rel(p);
    while (!p->had_error) {
        Token tok = peek(p);
        BinaryOp op;
        if (tok.kind == TOK_EQ) op = BINOP_EQ;
        else if (tok.kind == TOK_NEQ) op = BINOP_NEQ;
        else break;
        advance_tok(p);
        left = make_binary(p, tok, op, left, parse_rel(p));
    }
    return left;
}

static Expr* parse_and(Parser* p) {
    Expr* left = parse_eq(p);
    while (!p->had_error && peek(p).kind == TOK_AND) {
        Token tok = advance_tok(p);
        left = make_binary(p, tok, BINOP_AND, left, parse_eq(p));
    }
    return left;
}

static Expr* parse_or(Parser* p) {
    Expr* left = parse_and(p);
    while (!p->had_error && peek(p).kind == TOK_OR) {
        Token tok = advance_tok(p);
        left = make_binary(p, tok, BINOP_OR, left, parse_and(p));
    }
    return left;
}

static Expr* parse_assign(Parser* p) {
    Expr* left = parse_or(p);
    if (!p->had_error && peek(p).kind == TOK_ASSIGN) {
        Token tok = advance_tok(p);
        if (left->kind != EXPR_IDENT && left->kind != EXPR_FIELD && left->kind != EXPR_INDEX) {
            parse_error(p, tok, "invalid assignment target");
            return left;
        }
        Expr* right = parse_assign(p);
        Expr* e = new_expr(p, EXPR_ASSIGN, tok);
        e->as.assign.target = left;
        e->as.assign.value = right;
        return e;
    }
    return left;
}

static Expr* parse_expr(Parser* p) {
    return parse_assign(p);
}

/* =========================================================================
 * Statements
 * ========================================================================= */

static Stmt* new_stmt(Parser* p, StmtKind kind, Token tok) {
    Stmt* s = (Stmt*)rt_arena_alloc(p->arena, sizeof(Stmt));
    s->kind = kind;
    s->line = tok.line;
    s->col = tok.col;
    return s;
}

static bool is_var_decl_start(const Parser* p) {
    Token t0 = peek(p);
    if (t0.kind == TOK_KW_INT || t0.kind == TOK_KW_BYTE || t0.kind == TOK_KW_BOOLEAN) {
        return true;
    }
    if (t0.kind == TOK_IDENT) {
        Token t1 = peek_at(p, 1);
        if (t1.kind == TOK_IDENT) {
            return true;
        }
        if (t1.kind == TOK_LBRACKET && peek_at(p, 2).kind == TOK_RBRACKET) {
            return true;
        }
    }
    return false;
}

static Stmt* parse_var_decl(Parser* p) {
    Token tok = peek(p);
    Type* ty = parse_type(p, false);
    Token name = expect(p, TOK_IDENT, "expected variable name");
    Expr* init = NULL;
    if (!p->had_error && match(p, TOK_ASSIGN)) {
        init = parse_expr(p);
    }
    expect(p, TOK_SEMI, "expected ';' after variable declaration");
    Stmt* s = new_stmt(p, STMT_VAR_DECL, tok);
    s->as.var_decl.type = ty;
    s->as.var_decl.name = arena_strdup(p->arena, name.start, name.length);
    s->as.var_decl.init = init;
    return s;
}

static Stmt* parse_block(Parser* p) {
    Token lbrace = expect(p, TOK_LBRACE, "expected '{'");
    PtrVec stmts;
    ptr_vec_init(&stmts);
    while (!p->had_error && peek(p).kind != TOK_RBRACE && peek(p).kind != TOK_EOF) {
        ptr_vec_push(&stmts, parse_stmt(p));
    }
    expect(p, TOK_RBRACE, "expected '}'");
    Stmt* s = new_stmt(p, STMT_BLOCK, lbrace);
    s->as.block.count = stmts.count;
    s->as.block.stmts = (Stmt**)ptr_vec_to_arena(&stmts, p->arena, sizeof(Stmt*));
    return s;
}

static Stmt* parse_stmt(Parser* p) {
    Token tok = peek(p);
    if (tok.kind == TOK_LBRACE) {
        return parse_block(p);
    }
    if (tok.kind == TOK_KW_IF) {
        advance_tok(p);
        expect(p, TOK_LPAREN, "expected '(' after 'if'");
        Expr* cond = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after 'if' condition");
        Stmt* then_b = parse_stmt(p);
        Stmt* else_b = NULL;
        if (!p->had_error && match(p, TOK_KW_ELSE)) {
            else_b = parse_stmt(p);
        }
        Stmt* s = new_stmt(p, STMT_IF, tok);
        s->as.if_stmt.cond = cond;
        s->as.if_stmt.then_branch = then_b;
        s->as.if_stmt.else_branch = else_b;
        return s;
    }
    if (tok.kind == TOK_KW_WHILE) {
        advance_tok(p);
        expect(p, TOK_LPAREN, "expected '(' after 'while'");
        Expr* cond = parse_expr(p);
        expect(p, TOK_RPAREN, "expected ')' after 'while' condition");
        Stmt* body = parse_stmt(p);
        Stmt* s = new_stmt(p, STMT_WHILE, tok);
        s->as.while_stmt.cond = cond;
        s->as.while_stmt.body = body;
        return s;
    }
    if (tok.kind == TOK_KW_BREAK) {
        advance_tok(p);
        expect(p, TOK_SEMI, "expected ';' after 'break'");
        return new_stmt(p, STMT_BREAK, tok);
    }
    if (tok.kind == TOK_KW_CONTINUE) {
        advance_tok(p);
        expect(p, TOK_SEMI, "expected ';' after 'continue'");
        return new_stmt(p, STMT_CONTINUE, tok);
    }
    if (tok.kind == TOK_KW_RETURN) {
        advance_tok(p);
        Expr* val = NULL;
        if (!p->had_error && peek(p).kind != TOK_SEMI) {
            val = parse_expr(p);
        }
        expect(p, TOK_SEMI, "expected ';' after 'return'");
        Stmt* s = new_stmt(p, STMT_RETURN, tok);
        s->as.return_stmt.value = val;
        return s;
    }
    if (is_var_decl_start(p)) {
        return parse_var_decl(p);
    }
    Expr* expr = parse_expr(p);
    expect(p, TOK_SEMI, "expected ';' after expression");
    Stmt* s = new_stmt(p, STMT_EXPR, tok);
    s->as.expr_stmt.expr = expr;
    return s;
}

/* =========================================================================
 * Declarations (Classes, Fields, Constructors, Methods)
 * ========================================================================= */

static void parse_param_list(Parser* p, Param** params_out, int32_t* count_out) {
    expect(p, TOK_LPAREN, "expected '('");
    PtrVec params;
    ptr_vec_init(&params);
    if (!p->had_error && peek(p).kind != TOK_RPAREN) {
        do {
            Param* param = (Param*)rt_arena_alloc(p->arena, sizeof(Param));
            param->type = parse_type(p, false);
            Token name = expect(p, TOK_IDENT, "expected parameter name");
            param->name = arena_strdup(p->arena, name.start, name.length);
            param->line = name.line;
            param->col = name.col;
            ptr_vec_push(&params, param);
        } while (!p->had_error && match(p, TOK_COMMA));
    }
    expect(p, TOK_RPAREN, "expected ')' after parameter list");

    *count_out = params.count;
    if (params.count > 0) {
        Param* flat = (Param*)rt_arena_alloc(p->arena, (size_t)params.count * sizeof(Param));
        for (int32_t i = 0; i < params.count; i++) {
            flat[i] = *(Param*)params.items[i];
        }
        *params_out = flat;
    } else {
        *params_out = NULL;
    }
    free(params.items);
}

static ClassDecl* parse_class(Parser* p) {
    /* Optional visibility modifier (ignored semantically in Stage 0) */
    if (peek(p).kind == TOK_KW_PUBLIC || peek(p).kind == TOK_KW_PRIVATE) {
        advance_tok(p);
    }
    Token class_kw = expect(p, TOK_KW_CLASS, "expected 'class'");
    Token class_name_tok = expect(p, TOK_IDENT, "expected class name");
    expect(p, TOK_LBRACE, "expected '{' after class name");

    ClassDecl* cls = (ClassDecl*)rt_arena_alloc(p->arena, sizeof(ClassDecl));
    cls->name = arena_strdup(p->arena, class_name_tok.start, class_name_tok.length);
    cls->line = class_kw.line;
    cls->col = class_kw.col;

    PtrVec fields;
    PtrVec methods;
    ptr_vec_init(&fields);
    ptr_vec_init(&methods);

    while (!p->had_error && peek(p).kind != TOK_RBRACE && peek(p).kind != TOK_EOF) {
        /* Modifiers: [public|private] [static] [native] */
        if (peek(p).kind == TOK_KW_PUBLIC || peek(p).kind == TOK_KW_PRIVATE) {
            advance_tok(p);
        }
        bool is_static = match(p, TOK_KW_STATIC);
        bool is_native = match(p, TOK_KW_NATIVE);

        /* Check for constructor: ClassName(...) */
        if (!is_static && !is_native &&
            peek(p).kind == TOK_IDENT &&
            token_equals(peek(p), cls->name) &&
            peek_at(p, 1).kind == TOK_LPAREN) {
            Token ctor_tok = advance_tok(p);
            if (cls->constructor != NULL) {
                parse_error(p, ctor_tok, "multiple constructors are not supported in Stage 0");
                break;
            }
            MethodDecl* ctor = (MethodDecl*)rt_arena_alloc(p->arena, sizeof(MethodDecl));
            ctor->name = cls->name;
            ctor->return_type = make_type(p->arena, TY_VOID);
            ctor->is_constructor = true;
            ctor->owner = cls;
            ctor->line = ctor_tok.line;
            ctor->col = ctor_tok.col;
            parse_param_list(p, &ctor->params, &ctor->param_count);
            ctor->body = parse_block(p);
            cls->constructor = ctor;
            continue;
        }

        /* Otherwise: Field or Method */
        Type* member_type = parse_type(p, true);
        Token member_name = expect(p, TOK_IDENT, "expected field or method name");

        if (!p->had_error && peek(p).kind == TOK_LPAREN) {
            if (is_native && !is_static) {
                parse_error(p, member_name, "'native' methods must be 'static' in Stage 0");
                break;
            }
            MethodDecl* m = (MethodDecl*)rt_arena_alloc(p->arena, sizeof(MethodDecl));
            m->name = arena_strdup(p->arena, member_name.start, member_name.length);
            m->return_type = member_type;
            m->is_constructor = false;
            m->is_static = is_static;
            m->is_native = is_native;
            m->owner = cls;
            m->line = member_name.line;
            m->col = member_name.col;
            parse_param_list(p, &m->params, &m->param_count);
            if (is_native) {
                expect(p, TOK_SEMI, "expected ';' after native method declaration");
                m->body = NULL;
            } else {
                m->body = parse_block(p);
            }
            ptr_vec_push(&methods, m);
        } else {
            if (is_native) {
                parse_error(p, member_name, "fields cannot be 'native'");
                break;
            }
            if (member_type->kind == TY_VOID) {
                parse_error(p, member_name, "field cannot have type 'void'");
                break;
            }
            expect(p, TOK_SEMI, "expected ';' after field declaration");
            FieldDecl* f = (FieldDecl*)rt_arena_alloc(p->arena, sizeof(FieldDecl));
            f->type = member_type;
            f->name = arena_strdup(p->arena, member_name.start, member_name.length);
            f->is_static = is_static;
            f->line = member_name.line;
            f->col = member_name.col;
            ptr_vec_push(&fields, f);
        }
    }

    expect(p, TOK_RBRACE, "expected '}' at end of class");

    cls->field_count = fields.count;
    if (fields.count > 0) {
        FieldDecl* flat = (FieldDecl*)rt_arena_alloc(p->arena, (size_t)fields.count * sizeof(FieldDecl));
        for (int32_t i = 0; i < fields.count; i++) {
            flat[i] = *(FieldDecl*)fields.items[i];
        }
        cls->fields = flat;
    }
    free(fields.items);

    cls->method_count = methods.count;
    cls->methods = (MethodDecl**)ptr_vec_to_arena(&methods, p->arena, sizeof(MethodDecl*));

    return cls;
}

void parser_init(Parser* p, RtArena* arena, const char* filename, const char* src, int32_t len) {
    lexer_init(&p->lex, filename, src, len);
    p->arena = arena;
    p->had_error = false;
    p->lookahead[0] = lexer_next(&p->lex);
    p->lookahead[1] = lexer_next(&p->lex);
    p->lookahead[2] = lexer_next(&p->lex);
}

bool parser_parse_classes(Parser* p, ClassDecl*** classes_out, int32_t* count_out) {
    PtrVec classes;
    ptr_vec_init(&classes);

    while (!p->had_error && peek(p).kind != TOK_EOF) {
        ptr_vec_push(&classes, parse_class(p));
    }

    if (p->had_error) {
        free(classes.items);
        *classes_out = NULL;
        *count_out = 0;
        return false;
    }

    *count_out = classes.count;
    *classes_out = (ClassDecl**)ptr_vec_to_arena(&classes, p->arena, sizeof(ClassDecl*));
    return true;
}
