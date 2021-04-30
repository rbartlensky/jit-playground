#pragma once

#include "value.h"
#include "traces.h"

#include "cvector.h"

typedef struct JitState {
        TraceMap traces;
        cvector_vector_type(TraceList) open_traces;
} JitState;

JitState lsp_jit_new();

void lsp_jit_free(JitState self[static 1]);

void lsp_jit_trace_start(JitState self[static 1]);

void lsp_jit_record(JitState self[static 1], LspInstr i, LspValue *regs);

void lsp_jit_trace_end(JitState self[static 1], size_t func);
