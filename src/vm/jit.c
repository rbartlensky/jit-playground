#include "jit.h"

JitState lsp_jit_new() {
        JitState jit = {
                .traces = lsp_trace_map_new(),
        };
        return jit;
}

void lsp_jit_free(JitState self[static 1]) {
        lsp_trace_map_free(&self->traces);
}
