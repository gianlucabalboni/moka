/**
 * codegen.h — Stage 0 C11 code generator.
 *
 * Emits self-contained C11 code (typedefs, structs, prototypes, constructors,
 * methods, and C main wrapper) from a type-checked Stage 0 Program.
 */
#ifndef MOKA_CODEGEN_H
#define MOKA_CODEGEN_H

#include "ast.h"
#include <stdbool.h>
#include <stdio.h>

bool codegen_emit(const Program* prog, FILE* out, const char* runtime_include);

#endif /* MOKA_CODEGEN_H */
