#include "sema.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* name;
    Type* type;
} VarBinding;

typedef struct Scope {
    struct Scope* parent;
    VarBinding* vars;
    int32_t count;
    int32_t cap;
} Scope;

typedef struct {
    Program* prog;
    RtArena* arena;
    ClassDecl* current_class;
    MethodDecl* current_method;
    Scope* scope;
    int32_t loop_depth;
    bool had_error;

    /* Cached primitive types */
    Type* ty_void;
    Type* ty_int;
    Type* ty_byte;
    Type* ty_boolean;
    Type* ty_null;
} Sema;

static void sema_error(Sema* s, int32_t line, int32_t col, const char* fmt, ...) {
    if (s->had_error) {
        return;
    }
    s->had_error = true;
    fprintf(stderr, "line %d:%d: semantic error: ", line, col);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

static Type* make_prim_type(RtArena* arena, TypeKind kind) {
    Type* ty = (Type*)rt_arena_alloc(arena, sizeof(Type));
    ty->kind = kind;
    return ty;
}

static Type* make_class_type(RtArena* arena, ClassDecl* cls) {
    Type* ty = (Type*)rt_arena_alloc(arena, sizeof(Type));
    ty->kind = TY_CLASS;
    ty->class_name = cls->name;
    ty->resolved_class = cls;
    return ty;
}

/* =========================================================================
 * Scope Management
 * ========================================================================= */

static void push_scope(Sema* s) {
    Scope* sc = (Scope*)calloc(1, sizeof(Scope));
    if (!sc) {
        fprintf(stderr, "moka compiler error: out of memory\n");
        exit(1);
    }
    sc->parent = s->scope;
    s->scope = sc;
}

static void pop_scope(Sema* s) {
    Scope* sc = s->scope;
    if (sc) {
        s->scope = sc->parent;
        free(sc->vars);
        free(sc);
    }
}

static bool scope_declare(Sema* s, const char* name, Type* type, int32_t line, int32_t col) {
    Scope* sc = s->scope;
    for (int32_t i = 0; i < sc->count; i++) {
        if (strcmp(sc->vars[i].name, name) == 0) {
            sema_error(s, line, col, "duplicate variable '%s' in scope", name);
            return false;
        }
    }
    if (sc->count >= sc->cap) {
        int32_t new_cap = sc->cap == 0 ? 8 : sc->cap * 2;
        VarBinding* nb = (VarBinding*)realloc(sc->vars, (size_t)new_cap * sizeof(VarBinding));
        if (!nb) {
            fprintf(stderr, "moka compiler error: out of memory\n");
            exit(1);
        }
        sc->vars = nb;
        sc->cap = new_cap;
    }
    sc->vars[sc->count].name = name;
    sc->vars[sc->count].type = type;
    sc->count++;
    return true;
}

static Type* scope_lookup(const Sema* s, const char* name) {
    for (Scope* sc = s->scope; sc != NULL; sc = sc->parent) {
        for (int32_t i = 0; i < sc->count; i++) {
            if (strcmp(sc->vars[i].name, name) == 0) {
                return sc->vars[i].type;
            }
        }
    }
    return NULL;
}

/* =========================================================================
 * Class / Member Lookup & Type Helpers
 * ========================================================================= */

static ClassDecl* find_class(const Program* prog, const char* name) {
    for (int32_t i = 0; i < prog->class_count; i++) {
        if (strcmp(prog->classes[i]->name, name) == 0) {
            return prog->classes[i];
        }
    }
    return NULL;
}

static FieldDecl* find_field(const ClassDecl* cls, const char* name) {
    for (int32_t i = 0; i < cls->field_count; i++) {
        if (strcmp(cls->fields[i].name, name) == 0) {
            return &cls->fields[i];
        }
    }
    return NULL;
}

static MethodDecl* find_method(const ClassDecl* cls, const char* name) {
    for (int32_t i = 0; i < cls->method_count; i++) {
        if (strcmp(cls->methods[i]->name, name) == 0) {
            return cls->methods[i];
        }
    }
    return NULL;
}

static bool resolve_type(Sema* s, Type* ty, int32_t line, int32_t col) {
    if (!ty) {
        return true;
    }
    if (ty->kind == TY_CLASS) {
        if (!ty->resolved_class) {
            ClassDecl* cls = find_class(s->prog, ty->class_name);
            if (!cls) {
                sema_error(s, line, col, "unknown type '%s'", ty->class_name);
                return false;
            }
            ty->resolved_class = cls;
        }
    } else if (ty->kind == TY_ARRAY) {
        return resolve_type(s, ty->elem, line, col);
    }
    return true;
}

static bool is_numeric(const Type* ty) {
    return ty && (ty->kind == TY_INT || ty->kind == TY_BYTE);
}

static bool is_ref_type(const Type* ty) {
    return ty && (ty->kind == TY_CLASS || ty->kind == TY_ARRAY || ty->kind == TY_NULL);
}

static bool types_equal(const Type* a, const Type* b) {
    if (!a || !b || a->kind != b->kind) {
        return false;
    }
    if (a->kind == TY_CLASS) {
        return a->resolved_class == b->resolved_class;
    }
    if (a->kind == TY_ARRAY) {
        return types_equal(a->elem, b->elem);
    }
    return true;
}

static bool expr_assignable(const Type* target, const Expr* val_expr) {
    const Type* val = val_expr->inferred_type;
    if (!target || !val) {
        return false;
    }
    if (types_equal(target, val)) {
        return true;
    }
    /* byte and int are mutually assignable in Stage 0 (no cast syntax yet) */
    if ((target->kind == TY_INT && val->kind == TY_BYTE) ||
        (target->kind == TY_BYTE && val->kind == TY_INT)) {
        return true;
    }
    /* null is assignable to any class or array */
    if (val->kind == TY_NULL && (target->kind == TY_CLASS || target->kind == TY_ARRAY)) {
        return true;
    }
    return false;
}

static const char* type_name(const Type* ty) {
    if (!ty) return "<unknown>";
    switch (ty->kind) {
        case TY_VOID:    return "void";
        case TY_INT:     return "int";
        case TY_BYTE:    return "byte";
        case TY_BOOLEAN: return "boolean";
        case TY_NULL:    return "null";
        case TY_CLASS:   return ty->class_name ? ty->class_name : "<class>";
        case TY_ARRAY:   return "<array>";
    }
    return "<unknown>";
}

/* =========================================================================
 * Pass 1: Register Classes and Resolve Member Signatures
 * ========================================================================= */

static bool pass1_register(Sema* s) {
    Program* prog = s->prog;

    for (int32_t i = 0; i < prog->class_count; i++) {
        ClassDecl* cls = prog->classes[i];
        for (int32_t j = 0; j < i; j++) {
            if (strcmp(prog->classes[j]->name, cls->name) == 0) {
                sema_error(s, cls->line, cls->col, "duplicate class '%s'", cls->name);
                return false;
            }
        }
    }

    for (int32_t i = 0; i < prog->class_count; i++) {
        ClassDecl* cls = prog->classes[i];

        for (int32_t f = 0; f < cls->field_count; f++) {
            FieldDecl* fld = &cls->fields[f];
            for (int32_t prev = 0; prev < f; prev++) {
                if (strcmp(cls->fields[prev].name, fld->name) == 0) {
                    sema_error(s, fld->line, fld->col,
                               "duplicate field '%s' in class '%s'", fld->name, cls->name);
                    return false;
                }
            }
            if (!resolve_type(s, fld->type, fld->line, fld->col)) {
                return false;
            }
        }

        if (cls->constructor) {
            MethodDecl* ctor = cls->constructor;
            for (int32_t p = 0; p < ctor->param_count; p++) {
                if (!resolve_type(s, ctor->params[p].type,
                                  ctor->params[p].line, ctor->params[p].col)) {
                    return false;
                }
            }
        }

        for (int32_t m = 0; m < cls->method_count; m++) {
            MethodDecl* meth = cls->methods[m];
            for (int32_t prev = 0; prev < m; prev++) {
                if (strcmp(cls->methods[prev]->name, meth->name) == 0) {
                    sema_error(s, meth->line, meth->col,
                               "duplicate method '%s' in class '%s'", meth->name, cls->name);
                    return false;
                }
            }
            if (!resolve_type(s, meth->return_type, meth->line, meth->col)) {
                return false;
            }
            for (int32_t p = 0; p < meth->param_count; p++) {
                if (!resolve_type(s, meth->params[p].type,
                                  meth->params[p].line, meth->params[p].col)) {
                    return false;
                }
            }
        }
    }
    return true;
}

/* =========================================================================
 * Pass 2: Type-Check Expressions and Statements
 * ========================================================================= */

static void check_expr(Sema* s, Expr* e);
static void check_stmt(Sema* s, Stmt* st);

static void check_args(Sema* s, const char* callee_name, MethodDecl* target,
                       Expr** args, int32_t arg_count, int32_t line, int32_t col) {
    int32_t expected = target ? target->param_count : 0;
    if (arg_count != expected) {
        sema_error(s, line, col, "'%s' expects %d argument(s), got %d",
                   callee_name, expected, arg_count);
        return;
    }
    for (int32_t i = 0; i < arg_count; i++) {
        check_expr(s, args[i]);
        if (s->had_error) return;
        if (!expr_assignable(target->params[i].type, args[i])) {
            sema_error(s, args[i]->line, args[i]->col,
                       "argument %d of '%s' has incompatible type '%s' (expected '%s')",
                       i + 1, callee_name,
                       type_name(args[i]->inferred_type),
                       type_name(target->params[i].type));
            return;
        }
    }
}

static void check_expr(Sema* s, Expr* e) {
    if (!e || s->had_error) {
        return;
    }

    switch (e->kind) {
        case EXPR_INT_LIT:
            e->inferred_type = s->ty_int;
            return;
        case EXPR_CHAR_LIT:
            e->inferred_type = s->ty_byte;
            return;
        case EXPR_BOOL_LIT:
            e->inferred_type = s->ty_boolean;
            return;
        case EXPR_NULL:
            e->inferred_type = s->ty_null;
            return;
        case EXPR_STRING_LIT: {
            ClassDecl* str_cls = find_class(s->prog, "String");
            if (!str_cls) {
                sema_error(s, e->line, e->col,
                           "'String' class is required to use string literals");
                return;
            }
            e->inferred_type = make_class_type(s->arena, str_cls);
            return;
        }
        case EXPR_THIS:
            if (s->current_method && s->current_method->is_static) {
                sema_error(s, e->line, e->col, "'this' cannot be used in a static method");
                return;
            }
            e->inferred_type = make_class_type(s->arena, s->current_class);
            return;
        case EXPR_IDENT: {
            Type* local = scope_lookup(s, e->as.ident.name);
            if (local) {
                e->inferred_type = local;
                return;
            }
            ClassDecl* cls = find_class(s->prog, e->as.ident.name);
            if (cls) {
                e->as.ident.is_class_ref = true;
                e->inferred_type = make_class_type(s->arena, cls);
                return;
            }
            sema_error(s, e->line, e->col, "undeclared identifier '%s'", e->as.ident.name);
            return;
        }
        case EXPR_UNARY: {
            check_expr(s, e->as.unary.operand);
            if (s->had_error) return;
            Type* op_ty = e->as.unary.operand->inferred_type;
            if (e->as.unary.op == UNOP_NEG) {
                if (!is_numeric(op_ty)) {
                    sema_error(s, e->line, e->col, "unary '-' requires numeric operand");
                    return;
                }
                e->inferred_type = s->ty_int;
            } else {
                if (!op_ty || op_ty->kind != TY_BOOLEAN) {
                    sema_error(s, e->line, e->col, "unary '!' requires boolean operand");
                    return;
                }
                e->inferred_type = s->ty_boolean;
            }
            return;
        }
        case EXPR_BINARY: {
            check_expr(s, e->as.binary.left);
            check_expr(s, e->as.binary.right);
            if (s->had_error) return;
            Type* lt = e->as.binary.left->inferred_type;
            Type* rt = e->as.binary.right->inferred_type;
            BinaryOp op = e->as.binary.op;

            if (op == BINOP_ADD || op == BINOP_SUB || op == BINOP_MUL ||
                op == BINOP_DIV || op == BINOP_MOD) {
                if (!is_numeric(lt) || !is_numeric(rt)) {
                    sema_error(s, e->line, e->col, "arithmetic operator requires numeric operands");
                    return;
                }
                e->inferred_type = s->ty_int;
            } else if (op == BINOP_LT || op == BINOP_LE || op == BINOP_GT || op == BINOP_GE) {
                if (!is_numeric(lt) || !is_numeric(rt)) {
                    sema_error(s, e->line, e->col, "comparison operator requires numeric operands");
                    return;
                }
                e->inferred_type = s->ty_boolean;
            } else if (op == BINOP_EQ || op == BINOP_NEQ) {
                bool ok = (is_numeric(lt) && is_numeric(rt)) ||
                          (lt->kind == TY_BOOLEAN && rt->kind == TY_BOOLEAN) ||
                          (is_ref_type(lt) && is_ref_type(rt) &&
                           (types_equal(lt, rt) || lt->kind == TY_NULL || rt->kind == TY_NULL));
                if (!ok) {
                    sema_error(s, e->line, e->col, "incompatible types for equality comparison");
                    return;
                }
                e->inferred_type = s->ty_boolean;
            } else { /* BINOP_AND, BINOP_OR */
                if (!lt || !rt || lt->kind != TY_BOOLEAN || rt->kind != TY_BOOLEAN) {
                    sema_error(s, e->line, e->col, "logical operator requires boolean operands");
                    return;
                }
                e->inferred_type = s->ty_boolean;
            }
            return;
        }
        case EXPR_ASSIGN: {
            Expr* target = e->as.assign.target;
            check_expr(s, target);
            check_expr(s, e->as.assign.value);
            if (s->had_error) return;
            if (target->kind == EXPR_IDENT && target->as.ident.is_class_ref) {
                sema_error(s, target->line, target->col, "cannot assign to class name");
                return;
            }
            if (target->kind == EXPR_FIELD && target->as.field.is_array_length) {
                sema_error(s, target->line, target->col, "array '.length' is read-only");
                return;
            }
            if (!expr_assignable(target->inferred_type, e->as.assign.value)) {
                sema_error(s, e->line, e->col,
                           "cannot assign '%s' to '%s'",
                           type_name(e->as.assign.value->inferred_type),
                           type_name(target->inferred_type));
                return;
            }
            e->inferred_type = target->inferred_type;
            return;
        }
        case EXPR_FIELD: {
            Expr* obj = e->as.field.object;
            check_expr(s, obj);
            if (s->had_error) return;
            if (obj->kind == EXPR_IDENT && obj->as.ident.is_class_ref) {
                sema_error(s, e->line, e->col,
                           "cannot access field '%s' on class '%s' without an instance",
                           e->as.field.name, obj->as.ident.name);
                return;
            }
            Type* obj_ty = obj->inferred_type;
            if (obj_ty->kind == TY_ARRAY) {
                if (strcmp(e->as.field.name, "length") == 0) {
                    e->as.field.is_array_length = true;
                    e->inferred_type = s->ty_int;
                    return;
                }
                sema_error(s, e->line, e->col, "arrays only have '.length' property");
                return;
            }
            if (obj_ty->kind == TY_CLASS) {
                FieldDecl* fld = find_field(obj_ty->resolved_class, e->as.field.name);
                if (!fld) {
                    sema_error(s, e->line, e->col, "class '%s' has no field '%s'",
                               obj_ty->resolved_class->name, e->as.field.name);
                    return;
                }
                e->inferred_type = fld->type;
                return;
            }
            sema_error(s, e->line, e->col, "cannot access field on non-object type");
            return;
        }
        case EXPR_INDEX: {
            check_expr(s, e->as.index.array);
            check_expr(s, e->as.index.index);
            if (s->had_error) return;
            Type* arr_ty = e->as.index.array->inferred_type;
            if (!arr_ty || arr_ty->kind != TY_ARRAY) {
                sema_error(s, e->line, e->col, "indexing requires an array");
                return;
            }
            if (!is_numeric(e->as.index.index->inferred_type)) {
                sema_error(s, e->as.index.index->line, e->as.index.index->col,
                           "array index must be an integer");
                return;
            }
            e->inferred_type = arr_ty->elem;
            return;
        }
        case EXPR_CALL: {
            Expr* callee = e->as.call.callee;
            if (callee->kind == EXPR_FIELD) {
                Expr* obj = callee->as.field.object;
                const char* mname = callee->as.field.name;
                check_expr(s, obj);
                if (s->had_error) return;
                if (!obj->inferred_type || obj->inferred_type->kind != TY_CLASS) {
                    sema_error(s, e->line, e->col, "method call target must be a class or instance");
                    return;
                }
                ClassDecl* cls = obj->inferred_type->resolved_class;
                MethodDecl* m = find_method(cls, mname);
                if (!m) {
                    sema_error(s, e->line, e->col, "class '%s' has no method '%s'",
                               cls->name, mname);
                    return;
                }
                bool is_class_call = (obj->kind == EXPR_IDENT && obj->as.ident.is_class_ref);
                if (is_class_call && !m->is_static) {
                    sema_error(s, e->line, e->col,
                               "cannot call instance method '%s.%s' in static context",
                               cls->name, mname);
                    return;
                }
                if (!is_class_call && m->is_static) {
                    sema_error(s, e->line, e->col,
                               "static method '%s.%s' must be called on the class name",
                               cls->name, mname);
                    return;
                }
                check_args(s, mname, m, e->as.call.args, e->as.call.arg_count, e->line, e->col);
                e->as.call.resolved_method = m;
                e->inferred_type = m->return_type;
                return;
            }
            if (callee->kind == EXPR_IDENT) {
                const char* mname = callee->as.ident.name;
                MethodDecl* m = find_method(s->current_class, mname);
                if (!m || !m->is_static) {
                    sema_error(s, e->line, e->col,
                               "method '%s' not found (use 'this.%s()' for instance methods)",
                               mname, mname);
                    return;
                }
                check_args(s, mname, m, e->as.call.args, e->as.call.arg_count, e->line, e->col);
                e->as.call.resolved_method = m;
                e->inferred_type = m->return_type;
                return;
            }
            sema_error(s, e->line, e->col, "invalid call target");
            return;
        }
        case EXPR_NEW_OBJ: {
            ClassDecl* cls = find_class(s->prog, e->as.new_obj.class_name);
            if (!cls) {
                sema_error(s, e->line, e->col, "unknown class '%s'", e->as.new_obj.class_name);
                return;
            }
            e->as.new_obj.resolved_class = cls;
            check_args(s, cls->name, cls->constructor,
                       e->as.new_obj.args, e->as.new_obj.arg_count, e->line, e->col);
            e->inferred_type = make_class_type(s->arena, cls);
            return;
        }
        case EXPR_NEW_ARRAY: {
            if (!resolve_type(s, e->as.new_array.elem_type, e->line, e->col)) {
                return;
            }
            check_expr(s, e->as.new_array.size);
            if (s->had_error) return;
            if (!is_numeric(e->as.new_array.size->inferred_type)) {
                sema_error(s, e->as.new_array.size->line, e->as.new_array.size->col,
                           "array size must be an integer");
                return;
            }
            Type* arr = make_prim_type(s->arena, TY_ARRAY);
            arr->elem = e->as.new_array.elem_type;
            e->inferred_type = arr;
            return;
        }
    }
}

static void check_stmt(Sema* s, Stmt* st) {
    if (!st || s->had_error) {
        return;
    }

    switch (st->kind) {
        case STMT_BLOCK:
            push_scope(s);
            for (int32_t i = 0; i < st->as.block.count && !s->had_error; i++) {
                check_stmt(s, st->as.block.stmts[i]);
            }
            pop_scope(s);
            return;
        case STMT_VAR_DECL: {
            if (!resolve_type(s, st->as.var_decl.type, st->line, st->col)) {
                return;
            }
            if (st->as.var_decl.init) {
                check_expr(s, st->as.var_decl.init);
                if (s->had_error) return;
                if (!expr_assignable(st->as.var_decl.type, st->as.var_decl.init)) {
                    sema_error(s, st->line, st->col,
                               "cannot initialize '%s' of type '%s' with '%s'",
                               st->as.var_decl.name,
                               type_name(st->as.var_decl.type),
                               type_name(st->as.var_decl.init->inferred_type));
                    return;
                }
            }
            scope_declare(s, st->as.var_decl.name, st->as.var_decl.type, st->line, st->col);
            return;
        }
        case STMT_IF:
            check_expr(s, st->as.if_stmt.cond);
            if (s->had_error) return;
            if (!st->as.if_stmt.cond->inferred_type ||
                st->as.if_stmt.cond->inferred_type->kind != TY_BOOLEAN) {
                sema_error(s, st->as.if_stmt.cond->line, st->as.if_stmt.cond->col,
                           "'if' condition must be boolean");
                return;
            }
            check_stmt(s, st->as.if_stmt.then_branch);
            if (st->as.if_stmt.else_branch) {
                check_stmt(s, st->as.if_stmt.else_branch);
            }
            return;
        case STMT_WHILE:
            check_expr(s, st->as.while_stmt.cond);
            if (s->had_error) return;
            if (!st->as.while_stmt.cond->inferred_type ||
                st->as.while_stmt.cond->inferred_type->kind != TY_BOOLEAN) {
                sema_error(s, st->as.while_stmt.cond->line, st->as.while_stmt.cond->col,
                           "'while' condition must be boolean");
                return;
            }
            s->loop_depth++;
            check_stmt(s, st->as.while_stmt.body);
            s->loop_depth--;
            return;
        case STMT_BREAK:
            if (s->loop_depth <= 0) {
                sema_error(s, st->line, st->col, "'break' outside of loop");
            }
            return;
        case STMT_CONTINUE:
            if (s->loop_depth <= 0) {
                sema_error(s, st->line, st->col, "'continue' outside of loop");
            }
            return;
        case STMT_RETURN: {
            Type* ret = s->current_method->return_type;
            if (ret->kind == TY_VOID) {
                if (st->as.return_stmt.value) {
                    sema_error(s, st->line, st->col,
                               "void method/constructor cannot return a value");
                }
            } else {
                if (!st->as.return_stmt.value) {
                    sema_error(s, st->line, st->col, "non-void method must return a value");
                    return;
                }
                check_expr(s, st->as.return_stmt.value);
                if (s->had_error) return;
                if (!expr_assignable(ret, st->as.return_stmt.value)) {
                    sema_error(s, st->line, st->col,
                               "return value type '%s' incompatible with '%s'",
                               type_name(st->as.return_stmt.value->inferred_type),
                               type_name(ret));
                }
            }
            return;
        }
        case STMT_EXPR:
            check_expr(s, st->as.expr_stmt.expr);
            return;
    }
}

static void check_method(Sema* s, ClassDecl* cls, MethodDecl* m) {
    if (m->is_native || !m->body) {
        return;
    }
    s->current_class = cls;
    s->current_method = m;
    push_scope(s);
    for (int32_t i = 0; i < m->param_count && !s->had_error; i++) {
        scope_declare(s, m->params[i].name, m->params[i].type,
                      m->params[i].line, m->params[i].col);
    }
    /* Method body is a STMT_BLOCK, check its statements in parameter scope or nested scope */
    for (int32_t i = 0; i < m->body->as.block.count && !s->had_error; i++) {
        check_stmt(s, m->body->as.block.stmts[i]);
    }
    pop_scope(s);
}

bool sema_analyze(Program* prog, RtArena* arena) {
    Sema s;
    memset(&s, 0, sizeof(s));
    s.prog = prog;
    s.arena = arena;
    s.ty_void = make_prim_type(arena, TY_VOID);
    s.ty_int = make_prim_type(arena, TY_INT);
    s.ty_byte = make_prim_type(arena, TY_BYTE);
    s.ty_boolean = make_prim_type(arena, TY_BOOLEAN);
    s.ty_null = make_prim_type(arena, TY_NULL);

    if (!pass1_register(&s)) {
        return false;
    }

    for (int32_t i = 0; i < prog->class_count && !s.had_error; i++) {
        ClassDecl* cls = prog->classes[i];
        if (cls->constructor) {
            check_method(&s, cls, cls->constructor);
        }
        for (int32_t m = 0; m < cls->method_count && !s.had_error; m++) {
            check_method(&s, cls, cls->methods[m]);
        }
    }

    return !s.had_error;
}
