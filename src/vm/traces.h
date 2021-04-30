#pragma once

#include "compiler/opcodes.h"
#include "utils.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum NodeType {
        NODE_INSTR,
        NODE_LEN,
} NodeType;

typedef enum NodeMetadata {
        NODE_MD_TRUE,
        NODE_MD_FALSE,
        NODE_MD_NONE,
} NodeMetadata;

typedef struct TraceNode {
        union {
                LspInstr instr;
                size_t trace_len;
        };
        struct TraceNode* children[2];
        NodeType type;
        NodeMetadata metadata;
} TraceNode;

TraceNode lsp_trace_node_new(LspInstr instr, NodeMetadata md);

TraceNode lsp_trace_node_new_len(size_t len);

TraceNode* lsp_trace_node_add_child(TraceNode self[static 1], TraceNode node[static 1]);

TraceNode* lsp_trace_node_insert_child(TraceNode self[static 1], TraceNode node[static 1], bool left);

void lsp_trace_node_free(TraceNode self[static 1]);

void lsp_trace_node_print(TraceNode node[static 1]);

typedef struct TraceList {
        TraceNode *head;
        TraceNode *tail;
} TraceList;

TraceList lsp_trace_list_new(TraceNode instr);

TraceNode* lsp_trace_list_add(TraceList self[static 1], TraceNode instr);

void lsp_trace_list_free(TraceList self[static 1]);

typedef struct FuncTrace {
        size_t index;
        TraceNode *traces;
} FuncTrace;

typedef struct TraceMap {
        FuncTrace *traces;
        size_t len;
        size_t capacity;
} TraceMap;

TraceMap lsp_trace_map_new();

TraceNode* lsp_trace_map_get(const TraceMap self[static 1], size_t i);

void lsp_trace_map_insert(TraceMap self[static 1], size_t i, TraceList trace[static 1]);

void lsp_trace_map_free(TraceMap self[static 1]);
