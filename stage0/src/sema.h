/**
 * sema.h — Stage 0 two-pass semantic analyzer.
 *
 * Pass 1 registers all classes, fields, constructors, and method signatures.
 * Pass 2 type-checks method bodies, calls, arrays, and control flow.
 */
#ifndef MOKA_SEMA_H
#define MOKA_SEMA_H

#include "ast.h"
#include "runtime.h"
#include <stdbool.h>

bool sema_analyze(Program* prog, RtArena* arena);

#endif /* MOKA_SEMA_H */
