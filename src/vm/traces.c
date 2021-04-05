#include "traces.h"

#include <stdlib.h>
#include <string.h>

TraceNode lsp_trace_node_new(LspInstr instr) {
        TraceNode ret = {
                .instr = instr,
                .children = {NULL, NULL},
                .len = 0,
        };
        return ret;
}

TraceNode* lsp_trace_node_add_child(TraceNode self[static 1], LspInstr instr) {
        if (self->len == 2) {
                return NULL;
        }
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        *node = lsp_trace_node_new(instr);
        self->children[self->len++] = node;
        return node;
}

void lsp_trace_node_free(TraceNode self[static 1]) {
        for (uint8_t i = 0; i < self->len; ++i) {
                lsp_trace_node_free(self->children[i]);
                free(self->children[i]);
                self->children[i] = NULL;
        }
        self->len = 0;
}

TraceList lsp_trace_list_new(LspInstr instr) {
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        *node = lsp_trace_node_new(instr);
        TraceList list = {
                .head = node,
                .tail = node,
        };
        return list;
}

TraceNode* lsp_trace_list_add(TraceList self[static 1], LspInstr instr) {
        self->tail = lsp_trace_node_add_child(self->tail, instr);
        return self->tail;
}

void lsp_trace_list_free(TraceList self[static 1]) {
        lsp_trace_node_free(self->head);
        free(self->head);
}

TraceMap lsp_trace_map_new() {
        size_t len = 2;
        FuncTrace *mem = lsp_malloc(len * sizeof(FuncTrace));
        for (size_t i = 0; i < len; ++i) {
                mem[i].index = 0;
        }
        TraceMap map = {
                .traces = mem,
                .len = 0,
                .capacity = len,
        };
        return map;
}

TraceNode lsp_trace_map_get(const TraceMap self[static 1], size_t i) {
        size_t capacity = self->capacity;
        size_t map_index = i % capacity;
        FuncTrace p = self->traces[map_index];
        size_t seen = 0;
        while (p.index != 0 && p.index != i && seen < self->len) {
                map_index = (map_index + 1) % capacity;
                p = self->traces[map_index];
                seen++;
        }
        return p.traces;
}

static void resize(TraceMap self[static 1]) {
        size_t new_len = self->capacity * 2;
        self->traces = lsp_realloc(self->traces, new_len * sizeof(FuncTrace));
        self->capacity = new_len;
}

void lsp_trace_map_insert(TraceMap self[static 1], size_t i, TraceNode traces) {
        if (self->capacity == self->len) {
                resize(self);
        }
        size_t capacity = self->capacity;
        size_t map_index = i % capacity;
        FuncTrace p = self->traces[map_index];
        while (p.index != 0) {
                map_index = (map_index + 1) % capacity;
                p = self->traces[map_index];
        }
        self->traces[map_index].index = i;
        self->traces[map_index].traces = traces;
        self->len++;
}

void lsp_trace_map_free(TraceMap self[static 1]) {
        for (size_t i = 0; i < self->capacity; ++i) {
                FuncTrace t = self->traces[i];
                if (t.index != 0) {
                        lsp_trace_node_free(&t.traces);
                }
        }
        free(self->traces);
        self->traces = NULL;
}
