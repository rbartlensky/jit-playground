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
        TraceNode empty = lsp_trace_node_new(0, NODE_MD_NONE);
        cvector_push_back(self->open_traces, lsp_trace_list_new(empty));
}

void lsp_jit_record(JitState self[static 1], LspInstr i, LspValue *regs) {
        // skip jmp instructions for now
        uint8_t opcode = lsp_get_opcode(i);
        if (opcode == OP_JMP) {
                return;
        }
        size_t last = cvector_size(self->open_traces);
        if (last == 0) {
                return;
        }
        NodeMetadata md = NODE_MD_NONE;
        if (opcode == OP_TEST) {
                LspValue val = regs[lsp_get_arg1(i)];
                md = lsp_val_to_bool(val) == true ? NODE_MD_TRUE : NODE_MD_FALSE;
        }
        lsp_trace_list_add(&self->open_traces[last - 1], lsp_trace_node_new(i, md));
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
