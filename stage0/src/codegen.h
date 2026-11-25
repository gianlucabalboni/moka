#ifndef MOKA_CODEGEN_H
#define MOKA_CODEGEN_H

#include "ast.h"
#include <stdbool.h>
#include <stdio.h>

bool codegen_emit(const Program* prog, FILE* out, const char* runtime_include);

#endif /* MOKA_CODEGEN_H */
