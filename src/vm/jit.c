#include "jit.h"

JitState lsp_jit_new() {
        JitState jit = {
                .traces = lsp_trace_map_new(),
                .open_traces = NULL,
        };
        return jit;
}

void lsp_jit_free(JitState self[static 1]) {
        lsp_trace_map_free(&self->traces);
        cvector_free(self->open_traces);
}

void lsp_jit_trace_start(JitState self[static 1]) {
        cvector_push_back(self->open_traces, lsp_trace_list_new(0));
}

void lsp_jit_record(JitState self[static 1], LspInstr i) {
        size_t last = cvector_size(self->open_traces);
        if (last == 0) {
                return;
        }
        lsp_trace_list_add(&self->open_traces[last - 1], i);
}

void lsp_jit_trace_end(JitState self[static 1], size_t func) {
        size_t last = cvector_size(self->open_traces);
        if (last == 0 || func == 0) {
                return;
        }
        TraceList list = self->open_traces[last - 1];
        cvector_pop_back(self->open_traces);
        // list deallocation is handled by the map
        lsp_trace_map_insert(&self->traces, func, &list);
}
