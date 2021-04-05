#pragma once

#include "compiler/opcodes.h"
#include "utils.h"

#include <stddef.h>
#include <stdint.h>

typedef struct TraceNode {
        LspInstr instr;
        struct TraceNode* children[2];
        uint8_t len;
} TraceNode;

TraceNode lsp_trace_node_new(LspInstr instr);

TraceNode* lsp_trace_node_add_child(TraceNode self[static 1], LspInstr instr);

void lsp_trace_node_free(TraceNode self[static 1]);

typedef struct TraceList {
        TraceNode *head;
        TraceNode *tail;
} TraceList;

TraceList lsp_trace_list_new(LspInstr instr);

TraceNode* lsp_trace_list_add(TraceList self[static 1], LspInstr instr);

void lsp_trace_list_free(TraceList self[static 1]);

typedef struct FuncTrace {
        size_t index;
        TraceNode traces;
} FuncTrace;

typedef struct TraceMap {
        FuncTrace *traces;
        size_t len;
        size_t capacity;
} TraceMap;

TraceMap lsp_trace_map_new();

TraceNode lsp_trace_map_get(const TraceMap self[static 1], size_t i);

void lsp_trace_map_insert(TraceMap self[static 1], size_t i, TraceNode traces);

void lsp_trace_map_free(TraceMap self[static 1]);
