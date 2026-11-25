#ifndef MOKA_SEMA_H
#define MOKA_SEMA_H

#include "ast.h"
#include "runtime.h"
#include <stdbool.h>

bool sema_analyze(Program* prog, RtArena* arena);

#endif /* MOKA_SEMA_H */
