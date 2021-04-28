#include "traces.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

TraceNode lsp_trace_node_new(LspInstr instr) {
        TraceNode ret = {
                .instr = instr,
                .type = NODE_INSTR,
                .children = {NULL, NULL},
                .len = 0,
        };
        return ret;
}

TraceNode lsp_trace_node_new_len(size_t len) {
        TraceNode ret = {
                .trace_len = len,
                .type = NODE_LEN,
                .children = {NULL, NULL},
                .len = 0,
        };
        return ret;
}

TraceNode* lsp_trace_node_add_child(TraceNode self[static 1], TraceNode node[static 1]) {
        if (self->len == 2) {
                return NULL;
        }
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

static void print_trace_node(TraceNode self[static 1], size_t level) {
        if (self->len == 2) {
                printf("+");
        } else {
                printf("|");
        }
        for (size_t i = 0; i < level; ++i) {
                printf("-");
        }
        switch (self->type) {
                case NODE_LEN:
                        printf("Executed %ld times.\n", self->trace_len);
                        break;
                case NODE_INSTR:
                        lsp_print_instr(self->instr);
                        break;
        }
        for (uint8_t i = 0; i < self->len; ++i) {
                print_trace_node(self->children[i], level + i);
        }
}

void lsp_trace_node_print(TraceNode self[static 1]) {
        print_trace_node(self, 0);
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
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        *node = lsp_trace_node_new(instr);
        self->tail = lsp_trace_node_add_child(self->tail, node);
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
                mem[i].traces = NULL;
        }
        TraceMap map = {
                .traces = mem,
                .len = 0,
                .capacity = len,
        };
        return map;
}

TraceNode* lsp_trace_map_get(const TraceMap self[static 1], size_t i) {
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
        for (size_t i = self->capacity; i < new_len; ++i) {
                self->traces[i].index = 0;
                self->traces[i].traces = NULL;
        }
        self->capacity = new_len;
}

static bool equals(TraceNode n1[static 1], TraceNode n2[static 1]) {
        bool ret = false;
        if (n1->type == n2->type) {
                switch (n1->type) {
                        case NODE_INSTR:
                                ret = n1->instr == n2->instr;
                                break;
                        case NODE_LEN:
                                ret = n1->trace_len == n2->trace_len;
                                break;
                }
        }
        return ret;
}

/**
 * Merge a list of traced instructions into a tree of traces.
 *
 * \param `to` The tree of traces.
 * \param `from` The list of traced instructions.
 */
static void merge_traces(TraceNode to[static 1], TraceNode from[static 1]) {
        assert(to->type == from->type && to->type == NODE_INSTR && to->instr == from->instr);
        // first node is always an empty instruction
        {
                assert(from->instr == 0);
                TraceNode *q = from->children[0];
                free(from);
                from = q;
        }

        // at every iteration, check if any of the tree's children matches the
        // current node. If we find one, we go down one level in the tree.
        // If we don't, it means this is a new trace, so we add a new child to
        // the current node
        bool stop = true;
        while (from) {
                for (uint8_t i = 0; i < to->len; ++i) {
                        if (equals(to->children[i], from)) {
                                to = to->children[i];
                                TraceNode *q = from->children[0];
                                free(from);
                                from = q;
                                stop = false;
                                break;
                        }
                }
                if (stop) {
                        assert(to->len < 2);
                        to = lsp_trace_node_add_child(to, from);
                        break;
                } else {
                        stop = true;
                }
        }
        while (to->len > 0) {
                to = to->children[0];
        }
        if (to->type == NODE_LEN) {
                printf("Num times trace executed: %ld\n", ++to->trace_len);
        } else {
                TraceNode *node = lsp_malloc(sizeof(TraceNode));
                *node = lsp_trace_node_new_len(1);
                lsp_trace_node_add_child(to, node);
                printf("Num times trace executed: 1\n");
        }
}

void lsp_trace_map_insert(TraceMap self[static 1], size_t i, TraceList trace[static 1]) {
        TraceNode *res = lsp_trace_map_get(self, i);
        if (!res) {
                if (self->capacity == self->len) {
                        resize(self);
                }
                size_t capacity = self->capacity;
                size_t map_index = i % capacity;
                FuncTrace p = self->traces[map_index];
                while (p.index != 0 || p.index == i) {
                        map_index = (map_index + 1) % capacity;
                        p = self->traces[map_index];
                }
                self->traces[map_index].index = i;
                TraceNode *node = lsp_malloc(sizeof(TraceNode));
                *node = lsp_trace_node_new_len(1);
                trace->tail = lsp_trace_node_add_child(trace->tail, node);
                self->traces[map_index].traces = trace->head;
                self->len++;
        } else {
                merge_traces(res, trace->head);
        }
}

void lsp_trace_map_free(TraceMap self[static 1]) {
        for (size_t i = 0; i < self->capacity; ++i) {
                FuncTrace t = self->traces[i];
                if (t.index != 0) {
                        lsp_trace_node_print(t.traces);
                        lsp_trace_node_free(t.traces);
                }
        }
        free(self->traces);
        self->traces = NULL;
}
