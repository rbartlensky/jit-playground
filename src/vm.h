#pragma once

#include "compiler/gen.h"
#include "vm/jit.h"
#include "vm/value.h"

#include <cvector.h>

typedef struct LspVm {
        size_t regs_start;
        cvector_vector_type(LspValue) regs;
        uint64_t pc;
        LspState *state;
        size_t curr_fn;
        JitState jit;
} LspVm;

LspVm lsp_new_vm(LspState state[static 1]);

void lsp_cleanup_vm(LspVm vm[static 1]);

int lsp_interpret(LspVm vm[static 1]);
