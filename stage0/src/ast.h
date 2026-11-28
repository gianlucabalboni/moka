/**
 * ast.h — Stage 0 Abstract Syntax Tree definitions.
 *
 * Arena-allocated AST nodes for types, expressions, statements, fields,
 * methods, classes, and the top-level Program.
 */
#ifndef MOKA_AST_H
#define MOKA_AST_H

#include "runtime.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Param Param;
typedef struct FieldDecl FieldDecl;
typedef struct MethodDecl MethodDecl;
typedef struct ClassDecl ClassDecl;
typedef struct Program Program;

/* =========================================================================
 * Types
 * ========================================================================= */

typedef enum {
    TY_VOID = 0,
    TY_INT,
    TY_BYTE,
    TY_BOOLEAN,
    TY_CLASS,
    TY_ARRAY,
    TY_NULL
} TypeKind;

struct Type {
    TypeKind kind;
    const char* class_name;     /* For TY_CLASS */
    ClassDecl* resolved_class;  /* Populated by sema */
    Type* elem;                 /* For TY_ARRAY */
};

/* =========================================================================
 * Expressions
 * ========================================================================= */

typedef enum {
    EXPR_INT_LIT = 0,
    EXPR_CHAR_LIT,
    EXPR_BOOL_LIT,
    EXPR_STRING_LIT,
    EXPR_NULL,
    EXPR_THIS,
    EXPR_IDENT,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_FIELD,
    EXPR_INDEX,
    EXPR_CALL,
    EXPR_NEW_OBJ,
    EXPR_NEW_ARRAY
} ExprKind;

typedef enum {
    UNOP_NEG = 0,
    UNOP_NOT
} UnaryOp;

typedef enum {
    BINOP_ADD = 0,
    BINOP_SUB,
    BINOP_MUL,
    BINOP_DIV,
    BINOP_MOD,
    BINOP_EQ,
    BINOP_NEQ,
    BINOP_LT,
    BINOP_LE,
    BINOP_GT,
    BINOP_GE,
    BINOP_AND,
    BINOP_OR
} BinaryOp;

struct Expr {
    ExprKind kind;
    int32_t line;
    int32_t col;
    Type* inferred_type; /* Populated by sema */

    union {
        struct {
            int32_t val;
        } int_lit;

        struct {
            uint8_t val;
        } char_lit;

        struct {
            bool val;
        } bool_lit;

        struct {
            const char* bytes;
            int32_t len;
        } string_lit;

        struct {
            const char* name;
            bool is_class_ref; /* Populated by sema for ClassName.staticMethod() */
        } ident;

        struct {
            UnaryOp op;
            Expr* operand;
        } unary;

        struct {
            BinaryOp op;
            Expr* left;
            Expr* right;
        } binary;

        struct {
            Expr* target;
            Expr* value;
        } assign;

        struct {
            Expr* object;
            const char* name;
            bool is_array_length; /* Populated by sema for arr.length */
        } field;

        struct {
            Expr* array;
            Expr* index;
        } index;

        struct {
            Expr* callee;
            Expr** args;
            int32_t arg_count;
            MethodDecl* resolved_method; /* Populated by sema */
        } call;

        struct {
            const char* class_name;
            ClassDecl* resolved_class; /* Populated by sema */
            Expr** args;
            int32_t arg_count;
        } new_obj;

        struct {
            Type* elem_type;
            Expr* size;
        } new_array;
    } as;
};

/* =========================================================================
 * Statements
 * ========================================================================= */

typedef enum {
    STMT_BLOCK = 0,
    STMT_VAR_DECL,
    STMT_IF,
    STMT_WHILE,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_EXPR
} StmtKind;

struct Stmt {
    StmtKind kind;
    int32_t line;
    int32_t col;

    union {
        struct {
            Stmt** stmts;
            int32_t count;
        } block;

        struct {
            Type* type;
            const char* name;
            Expr* init; /* Nullable */
        } var_decl;

        struct {
            Expr* cond;
            Stmt* then_branch;
            Stmt* else_branch; /* Nullable */
        } if_stmt;

        struct {
            Expr* cond;
            Stmt* body;
        } while_stmt;

        struct {
            Expr* value; /* Nullable */
        } return_stmt;

        struct {
            Expr* expr;
        } expr_stmt;
    } as;
};

/* =========================================================================
 * Declarations
 * ========================================================================= */

struct Param {
    Type* type;
    const char* name;
    int32_t line;
    int32_t col;
};

struct FieldDecl {
    Type* type;
    const char* name;
    bool is_static;
    int32_t line;
    int32_t col;
};

struct MethodDecl {
    const char* name;
    Type* return_type;
    bool is_constructor;
    bool is_static;
    bool is_native;
    Param* params;
    int32_t param_count;
    Stmt* body; /* NULL if native */
    ClassDecl* owner; /* Populated by parser/sema */
    int32_t line;
    int32_t col;
};

struct ClassDecl {
    const char* name;
    FieldDecl* fields;
    int32_t field_count;
    MethodDecl* constructor; /* Nullable */
    MethodDecl** methods;
    int32_t method_count;
    int32_t line;
    int32_t col;
};

struct Program {
    ClassDecl** classes;
    int32_t class_count;
};

#endif /* MOKA_AST_H */
