#include "vm.h"

LspVm lsp_new_vm(LspState state[static 1]) {
        cvector_vector_type(uint64_t) regs = NULL;
        for (size_t i = 0; i < 256; ++i) {
                cvector_push_back(regs, 0);
        }
        LspVm vm = {
                .regs = regs,
                .pc = 0,
                .state = state,
        };
        return vm;
}

void lsp_cleanup_vm(LspVm vm[static 1]) {
        cvector_free(vm->regs);
}

int lsp_interpret(LspVm vm[static 1]) {
        LspState *state = vm->state;
        size_t len = cvector_size(vm->state->funcs[0].instrs);
        while (vm->pc < len) {
                LspInstr i = vm->state->funcs[0].instrs[vm->pc];
                switch (lsp_get_opcode(i)) {
                        case OP_LDC:
                                vm->regs[lsp_get_arg1(i)] = state->ints[lsp_get_long_arg(i)];
                                vm->pc++;
                                break;
                        case OP_ADD:
                                vm->regs[lsp_get_arg1(i)] = vm->regs[lsp_get_arg2(i)] + vm->regs[lsp_get_arg3(i)];
                                vm->pc++;
                                break;
                }
        }
        return 0;
}
