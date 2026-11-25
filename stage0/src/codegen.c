#include "codegen.h"
#include <string.h>

typedef struct {
    const Program* prog;
    FILE* out;
    int indent;
    const MethodDecl* current_method;
} Codegen;

static void emit_indent(Codegen* cg) {
    for (int i = 0; i < cg->indent; i++) {
        fputs("    ", cg->out);
    }
}

static void emit_c_type(FILE* out, const Type* ty) {
    if (!ty) {
        fputs("void", out);
        return;
    }
    switch (ty->kind) {
        case TY_VOID:    fputs("void", out); break;
        case TY_INT:     fputs("int32_t", out); break;
        case TY_BYTE:    fputs("uint8_t", out); break;
        case TY_BOOLEAN: fputs("bool", out); break;
        case TY_CLASS:   fprintf(out, "%s*", ty->resolved_class ? ty->resolved_class->name : ty->class_name); break;
        case TY_ARRAY:   fputs("RtArray*", out); break;
        case TY_NULL:    fputs("void*", out); break;
    }
}

static void emit_zero_value(FILE* out, const Type* ty) {
    switch (ty->kind) {
        case TY_INT:
        case TY_BYTE:    fputs("0", out); break;
        case TY_BOOLEAN: fputs("false", out); break;
        case TY_CLASS:
        case TY_ARRAY:
        case TY_NULL:    fputs("NULL", out); break;
        case TY_VOID:    break;
    }
}

static void emit_c_string_literal(FILE* out, const char* bytes, int32_t len) {
    fputc('"', out);
    for (int32_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)bytes[i];
        switch (c) {
            case '\n': fputs("\\n", out); break;
            case '\t': fputs("\\t", out); break;
            case '\r': fputs("\\r", out); break;
            case '\\': fputs("\\\\", out); break;
            case '"':  fputs("\\\"", out); break;
            default:
                if (c < 32 || c >= 127) {
                    fprintf(out, "\\%03o", c);
                } else {
                    fputc((char)c, out);
                }
                break;
        }
    }
    fputc('"', out);
}

static void emit_expr(Codegen* cg, const Expr* e);

static const char* binary_op_str(BinaryOp op) {
    switch (op) {
        case BINOP_ADD: return "+";
        case BINOP_SUB: return "-";
        case BINOP_MUL: return "*";
        case BINOP_DIV: return "/";
        case BINOP_MOD: return "%";
        case BINOP_EQ:  return "==";
        case BINOP_NEQ: return "!=";
        case BINOP_LT:  return "<";
        case BINOP_LE:  return "<=";
        case BINOP_GT:  return ">";
        case BINOP_GE:  return ">=";
        case BINOP_AND: return "&&";
        case BINOP_OR:  return "||";
    }
    return "+";
}

static void emit_expr(Codegen* cg, const Expr* e) {
    FILE* out = cg->out;
    switch (e->kind) {
        case EXPR_INT_LIT:
            fprintf(out, "%d", e->as.int_lit.val);
            return;
        case EXPR_CHAR_LIT:
            fprintf(out, "((uint8_t)%u)", (unsigned)e->as.char_lit.val);
            return;
        case EXPR_BOOL_LIT:
            fputs(e->as.bool_lit.val ? "true" : "false", out);
            return;
        case EXPR_NULL:
            fputs("NULL", out);
            return;
        case EXPR_THIS:
            fputs("this", out);
            return;
        case EXPR_IDENT:
            fputs(e->as.ident.name, out);
            return;
        case EXPR_STRING_LIT:
            fputs("String_new(rt_byte_array_from_literal(", out);
            emit_c_string_literal(out, e->as.string_lit.bytes, e->as.string_lit.len);
            fprintf(out, ", %d))", e->as.string_lit.len);
            return;
        case EXPR_UNARY:
            fputc('(', out);
            fputs(e->as.unary.op == UNOP_NEG ? "-" : "!", out);
            emit_expr(cg, e->as.unary.operand);
            fputc(')', out);
            return;
        case EXPR_BINARY:
            fputc('(', out);
            emit_expr(cg, e->as.binary.left);
            fprintf(out, " %s ", binary_op_str(e->as.binary.op));
            emit_expr(cg, e->as.binary.right);
            fputc(')', out);
            return;
        case EXPR_ASSIGN:
            fputc('(', out);
            emit_expr(cg, e->as.assign.target);
            fputs(" = ", out);
            emit_expr(cg, e->as.assign.value);
            fputc(')', out);
            return;
        case EXPR_FIELD: {
            const Expr* obj = e->as.field.object;
            if (e->as.field.is_array_length) {
                fputs("((RtArray*)rt_null_check(", out);
                emit_expr(cg, obj);
                fputs(", \"length\"))->length", out);
            } else {
                const char* cname = obj->inferred_type->resolved_class->name;
                if (obj->kind == EXPR_THIS) {
                    fprintf(out, "this->%s", e->as.field.name);
                } else {
                    fprintf(out, "((%s*)rt_null_check(", cname);
                    emit_expr(cg, obj);
                    fprintf(out, ", \"%s.%s\"))->%s", cname, e->as.field.name, e->as.field.name);
                }
            }
            return;
        }
        case EXPR_INDEX: {
            fputs("(*(", out);
            emit_c_type(out, e->inferred_type);
            fputs("*)rt_array_elem_ptr(", out);
            emit_expr(cg, e->as.index.array);
            fputs(", ", out);
            emit_expr(cg, e->as.index.index);
            fputs("))", out);
            return;
        }
        case EXPR_CALL: {
            const MethodDecl* m = e->as.call.resolved_method;
            if (m->is_native) {
                fprintf(out, "%s(", m->name);
                for (int32_t i = 0; i < e->as.call.arg_count; i++) {
                    if (i > 0) fputs(", ", out);
                    emit_expr(cg, e->as.call.args[i]);
                }
                fputc(')', out);
            } else if (m->is_static) {
                fprintf(out, "%s_%s(", m->owner->name, m->name);
                for (int32_t i = 0; i < e->as.call.arg_count; i++) {
                    if (i > 0) fputs(", ", out);
                    emit_expr(cg, e->as.call.args[i]);
                }
                fputc(')', out);
            } else {
                const Expr* obj = e->as.call.callee->as.field.object;
                fprintf(out, "%s_%s(", m->owner->name, m->name);
                if (obj->kind == EXPR_THIS) {
                    fputs("this", out);
                } else {
                    fprintf(out, "(%s*)rt_null_check(", m->owner->name);
                    emit_expr(cg, obj);
                    fprintf(out, ", \"%s.%s\")", m->owner->name, m->name);
                }
                for (int32_t i = 0; i < e->as.call.arg_count; i++) {
                    fputs(", ", out);
                    emit_expr(cg, e->as.call.args[i]);
                }
                fputc(')', out);
            }
            return;
        }
        case EXPR_NEW_OBJ:
            fprintf(out, "%s_new(", e->as.new_obj.resolved_class->name);
            for (int32_t i = 0; i < e->as.new_obj.arg_count; i++) {
                if (i > 0) fputs(", ", out);
                emit_expr(cg, e->as.new_obj.args[i]);
            }
            fputc(')', out);
            return;
        case EXPR_NEW_ARRAY: {
            const Type* elem = e->as.new_array.elem_type;
            bool is_ref = (elem->kind == TY_CLASS || elem->kind == TY_ARRAY);
            fputs("rt_array_new(", out);
            emit_expr(cg, e->as.new_array.size);
            fputs(", (int32_t)sizeof(", out);
            emit_c_type(out, elem);
            fprintf(out, "), %s)", is_ref ? "true" : "false");
            return;
        }
    }
}

static void emit_stmt(Codegen* cg, const Stmt* st) {
    FILE* out = cg->out;
    switch (st->kind) {
        case STMT_BLOCK:
            emit_indent(cg);
            fputs("{\n", out);
            cg->indent++;
            for (int32_t i = 0; i < st->as.block.count; i++) {
                emit_stmt(cg, st->as.block.stmts[i]);
            }
            cg->indent--;
            emit_indent(cg);
            fputs("}\n", out);
            return;
        case STMT_VAR_DECL:
            emit_indent(cg);
            emit_c_type(out, st->as.var_decl.type);
            fprintf(out, " %s = ", st->as.var_decl.name);
            if (st->as.var_decl.init) {
                emit_expr(cg, st->as.var_decl.init);
            } else {
                emit_zero_value(out, st->as.var_decl.type);
            }
            fputs(";\n", out);
            return;
        case STMT_IF:
            emit_indent(cg);
            fputs("if (", out);
            emit_expr(cg, st->as.if_stmt.cond);
            fputs(") ", out);
            if (st->as.if_stmt.then_branch->kind == STMT_BLOCK) {
                fputs("{\n", out);
                cg->indent++;
                const Stmt* b = st->as.if_stmt.then_branch;
                for (int32_t i = 0; i < b->as.block.count; i++) {
                    emit_stmt(cg, b->as.block.stmts[i]);
                }
                cg->indent--;
                emit_indent(cg);
                fputc('}', out);
            } else {
                fputc('\n', out);
                cg->indent++;
                emit_stmt(cg, st->as.if_stmt.then_branch);
                cg->indent--;
            }
            if (st->as.if_stmt.else_branch) {
                if (st->as.if_stmt.then_branch->kind == STMT_BLOCK) {
                    fputs(" else ", out);
                } else {
                    emit_indent(cg);
                    fputs("else ", out);
                }
                if (st->as.if_stmt.else_branch->kind == STMT_BLOCK) {
                    fputs("{\n", out);
                    cg->indent++;
                    const Stmt* b = st->as.if_stmt.else_branch;
                    for (int32_t i = 0; i < b->as.block.count; i++) {
                        emit_stmt(cg, b->as.block.stmts[i]);
                    }
                    cg->indent--;
                    emit_indent(cg);
                    fputs("}\n", out);
                } else {
                    fputc('\n', out);
                    cg->indent++;
                    emit_stmt(cg, st->as.if_stmt.else_branch);
                    cg->indent--;
                }
            } else {
                if (st->as.if_stmt.then_branch->kind == STMT_BLOCK) {
                    fputc('\n', out);
                }
            }
            return;
        case STMT_WHILE:
            emit_indent(cg);
            fputs("while (", out);
            emit_expr(cg, st->as.while_stmt.cond);
            fputs(") ", out);
            if (st->as.while_stmt.body->kind == STMT_BLOCK) {
                fputs("{\n", out);
                cg->indent++;
                const Stmt* b = st->as.while_stmt.body;
                for (int32_t i = 0; i < b->as.block.count; i++) {
                    emit_stmt(cg, b->as.block.stmts[i]);
                }
                cg->indent--;
                emit_indent(cg);
                fputs("}\n", out);
            } else {
                fputc('\n', out);
                cg->indent++;
                emit_stmt(cg, st->as.while_stmt.body);
                cg->indent--;
            }
            return;
        case STMT_BREAK:
            emit_indent(cg);
            fputs("break;\n", out);
            return;
        case STMT_CONTINUE:
            emit_indent(cg);
            fputs("continue;\n", out);
            return;
        case STMT_RETURN:
            emit_indent(cg);
            if (cg->current_method && cg->current_method->is_constructor) {
                fputs("return this;\n", out);
            } else if (st->as.return_stmt.value) {
                fputs("return ", out);
                emit_expr(cg, st->as.return_stmt.value);
                fputs(";\n", out);
            } else {
                fputs("return;\n", out);
            }
            return;
        case STMT_EXPR:
            emit_indent(cg);
            emit_expr(cg, st->as.expr_stmt.expr);
            fputs(";\n", out);
            return;
    }
}

static void emit_method_sig(Codegen* cg, const ClassDecl* cls, const MethodDecl* m) {
    FILE* out = cg->out;
    if (m && m->is_constructor) {
        fprintf(out, "%s* %s_new(", cls->name, cls->name);
        if (m->param_count == 0) {
            fputs("void", out);
        } else {
            for (int32_t i = 0; i < m->param_count; i++) {
                if (i > 0) fputs(", ", out);
                emit_c_type(out, m->params[i].type);
                fprintf(out, " %s", m->params[i].name);
            }
        }
        fputc(')', out);
        return;
    }

    if (!m) {
        /* Default zero-arg constructor */
        fprintf(out, "%s* %s_new(void)", cls->name, cls->name);
        return;
    }

    emit_c_type(out, m->return_type);
    fprintf(out, " %s_%s(", cls->name, m->name);
    bool first = true;
    if (!m->is_static) {
        fprintf(out, "%s* this", cls->name);
        first = false;
    }
    for (int32_t i = 0; i < m->param_count; i++) {
        if (!first) fputs(", ", out);
        first = false;
        emit_c_type(out, m->params[i].type);
        fprintf(out, " %s", m->params[i].name);
    }
    if (first) {
        fputs("void", out);
    }
    fputc(')', out);
}

static const MethodDecl* find_entry_main(const Program* prog) {
    for (int32_t i = 0; i < prog->class_count; i++) {
        const ClassDecl* cls = prog->classes[i];
        if (strcmp(cls->name, "Main") == 0) {
            for (int32_t m = 0; m < cls->method_count; m++) {
                if (cls->methods[m]->is_static &&
                    strcmp(cls->methods[m]->name, "main") == 0 &&
                    cls->methods[m]->param_count == 0) {
                    return cls->methods[m];
                }
            }
        }
    }
    return NULL;
}

bool codegen_emit(const Program* prog, FILE* out, const char* runtime_include) {
    Codegen cg;
    cg.prog = prog;
    cg.out = out;
    cg.indent = 0;
    cg.current_method = NULL;

    fprintf(out, "/* Generated by Moka Stage 0 Compiler */\n");
    fprintf(out, "#include \"%s\"\n\n", runtime_include ? runtime_include : "runtime.h");

    /* 1. Forward declarations */
    for (int32_t i = 0; i < prog->class_count; i++) {
        const char* name = prog->classes[i]->name;
        fprintf(out, "typedef struct %s %s;\n", name, name);
    }
    fputc('\n', out);

    /* 2. Struct definitions */
    for (int32_t i = 0; i < prog->class_count; i++) {
        const ClassDecl* cls = prog->classes[i];
        fprintf(out, "struct %s {\n", cls->name);
        fputs("    Obj header;\n", out);
        for (int32_t f = 0; f < cls->field_count; f++) {
            fputs("    ", out);
            emit_c_type(out, cls->fields[f].type);
            fprintf(out, " %s;\n", cls->fields[f].name);
        }
        fputs("};\n\n", out);
    }

    /* 3. Function prototypes */
    for (int32_t i = 0; i < prog->class_count; i++) {
        const ClassDecl* cls = prog->classes[i];
        emit_method_sig(&cg, cls, cls->constructor);
        fputs(";\n", out);
        for (int32_t m = 0; m < cls->method_count; m++) {
            if (!cls->methods[m]->is_native) {
                emit_method_sig(&cg, cls, cls->methods[m]);
                fputs(";\n", out);
            }
        }
    }
    fputc('\n', out);

    /* 4. Function definitions */
    for (int32_t i = 0; i < prog->class_count; i++) {
        const ClassDecl* cls = prog->classes[i];

        /* Constructor */
        emit_method_sig(&cg, cls, cls->constructor);
        fputs(" {\n", out);
        fprintf(out, "    %s* this = (%s*)rt_alloc_obj(sizeof(%s), OBJ_INSTANCE);\n",
                cls->name, cls->name, cls->name);
        if (cls->constructor && cls->constructor->body) {
            cg.current_method = cls->constructor;
            cg.indent = 1;
            const Stmt* body = cls->constructor->body;
            for (int32_t s = 0; s < body->as.block.count; s++) {
                emit_stmt(&cg, body->as.block.stmts[s]);
            }
            cg.indent = 0;
            cg.current_method = NULL;
        }
        fputs("    return this;\n", out);
        fputs("}\n\n", out);

        /* Methods */
        for (int32_t m = 0; m < cls->method_count; m++) {
            const MethodDecl* meth = cls->methods[m];
            if (meth->is_native) {
                continue;
            }
            emit_method_sig(&cg, cls, meth);
            fputs(" {\n", out);
            cg.current_method = meth;
            cg.indent = 1;
            if (!meth->is_static) {
                /* Silence unused 'this' if method doesn't reference it */
                fputs("    (void)this;\n", out);
            }
            for (int32_t p = 0; p < meth->param_count; p++) {
                fprintf(out, "    (void)%s;\n", meth->params[p].name);
            }
            if (meth->body) {
                for (int32_t s = 0; s < meth->body->as.block.count; s++) {
                    emit_stmt(&cg, meth->body->as.block.stmts[s]);
                }
            }
            cg.indent = 0;
            cg.current_method = NULL;
            fputs("}\n\n", out);
        }
    }

    /* 5. C entry point if Main.main() exists */
    const MethodDecl* entry = find_entry_main(prog);
    if (entry) {
        fputs("int main(int argc, char** argv) {\n", out);
        fputs("    rt_init(argc, argv);\n", out);
        if (entry->return_type->kind == TY_INT) {
            fputs("    int32_t exit_code = Main_main();\n", out);
            fputs("    rt_shutdown();\n", out);
            fputs("    return (int)exit_code;\n", out);
        } else {
            fputs("    Main_main();\n", out);
            fputs("    rt_shutdown();\n", out);
            fputs("    return 0;\n", out);
        }
        fputs("}\n", out);
    }

    return true;
}
