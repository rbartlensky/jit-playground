#pragma once

#include "compiler/gen.h"

#include <cvector.h>

typedef struct LspVm {
        cvector_vector_type(uint64_t) regs;
        uint64_t pc;
        LspState *state;
} LspVm;

LspVm lsp_new_vm(LspState state[static 1]);

void lsp_cleanup_vm(LspVm vm[static 1]);

int lsp_interpret(LspVm vm[static 1]);
