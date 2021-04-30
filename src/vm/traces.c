#include "traces.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define HOT_TRACE_COUNT 4

TraceNode lsp_trace_node_new(LspInstr instr, NodeMetadata md) {
        TraceNode ret = {
                .instr = instr,
                .type = NODE_INSTR,
                .children = {NULL, NULL},
                .metadata = md,
        };
        return ret;
}

TraceNode lsp_trace_node_new_len(size_t len) {
        TraceNode ret = {
                .trace_len = len,
                .type = NODE_LEN,
                .children = {NULL, NULL},
                .metadata = NODE_MD_NONE,
        };
        return ret;
}

TraceNode* lsp_trace_node_add_child(TraceNode self[static 1], TraceNode node[static 1]) {
        for (uint8_t i = 0; i < 2; ++i) {
                if (!self->children[i]) {
                        self->children[i] = node;
                        return node;
                }
        }
        return NULL;
}


TraceNode* lsp_trace_node_insert_child(TraceNode self[static 1], TraceNode node[static 1], bool right) {
        uint8_t i = right ? 1 : 0;
        if (self->children[i]) {
                lsp_trace_node_free(self->children[i]);
                free(self->children[i]);
        }
        self->children[i] = node;
        return node;
}

void lsp_trace_node_free(TraceNode self[static 1]) {
        for (uint8_t i = 0; i < 2; ++i) {
                if (self->children[i]) {
                        lsp_trace_node_free(self->children[i]);
                        free(self->children[i]);
                        self->children[i] = NULL;
                }
        }
}

static TraceNode* clone_trace_node(TraceNode self[static 1]) {
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        TraceNode data = {
                .type = self->type,
                .children = {NULL, NULL},
                .metadata = self->metadata,
        };
        *node = data;
        if (self->type == NODE_INSTR) {
                node->instr = self->instr;
        } else {
                node->trace_len = self->trace_len;
        }
        return node;

}

static void print_trace_node(TraceNode self[static 1], size_t level) {
        printf("|");
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
        for (uint8_t i = 0; i < 2; ++i) {
                if (self->children[i]) {
                        print_trace_node(self->children[i], level + 1);
                }
        }
}

void lsp_trace_node_print(TraceNode self[static 1]) {
        print_trace_node(self, 0);
}

TraceList lsp_trace_list_new(TraceNode instr) {
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        *node = instr;
        TraceList list = {
                .head = node,
                .tail = node,
        };
        return list;
}

TraceNode* lsp_trace_list_add(TraceList self[static 1], TraceNode instr) {
        TraceNode *node = lsp_malloc(sizeof(TraceNode));
        *node = instr;
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
        TraceNode *trace_root = from;
        TraceNode *prev_to = to, *prev_from = from;

        // metadata == False means that we took the false branch, which is the
        // same as the going down the sub-tree of our "right" child
        while (from) {
                // when `to` is NULL, it means our trace went on a new path of execution
                if (to && equals(to, from)) {
                        uint8_t path = 1;
                        if (from->metadata == NODE_MD_NONE || from->metadata == NODE_MD_TRUE) {
                                path = 0;
                        }
                        prev_to = to;
                        to = to->children[path];
                        prev_from = from;
                        from = from->children[0];
                } else {
                        assert(!to);
                        bool right = true;
                        if (prev_from->metadata == NODE_MD_NONE || prev_from->metadata == NODE_MD_TRUE) {
                                right = false;
                        }
                        prev_to = lsp_trace_node_insert_child(prev_to, clone_trace_node(from), right);
                        prev_from = from;
                        from = from->children[0];
                }
        }
        if (to && to->type == NODE_LEN) {
                if (++to->trace_len == HOT_TRACE_COUNT) {
                        // TODO:
                        //   * convert "test" instrs into guards
                        //   * compile into LLVM BC
                        //   * execute compiled BC next time function is called
                        printf("THIS IS A HOT TRACE! COMPILE IT!\n");
                }
        } else {
                TraceNode *node = lsp_malloc(sizeof(TraceNode));
                *node = lsp_trace_node_new_len(1);
                lsp_trace_node_add_child(prev_to, node);
        }

        lsp_trace_node_free(trace_root);
        free(trace_root);
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
                *node = lsp_trace_node_new(0, NODE_MD_NONE);
                self->traces[map_index].traces = node;
                res = node;
                self->len++;
        }
        merge_traces(res, trace->head);
}

void lsp_trace_map_free(TraceMap self[static 1]) {
        for (size_t i = 0; i < self->capacity; ++i) {
                FuncTrace t = self->traces[i];
                if (t.index != 0) {
                        lsp_trace_node_print(t.traces);
                        lsp_trace_node_free(t.traces);
                        free(t.traces);
                }
        }
        free(self->traces);
        self->traces = NULL;
}
