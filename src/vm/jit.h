#pragma once

#include "traces.h"

typedef struct JitState {
        TraceMap traces;
} JitState;

JitState lsp_jit_new();

void lsp_jit_free(JitState self[static 1]);
