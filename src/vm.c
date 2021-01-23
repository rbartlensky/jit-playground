#include "vm.h"

LspVm lsp_new_vm(LspState state[static 1]) {
        cvector_vector_type(LspValue) regs = NULL;
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

static LspValue add(LspValue v1, LspValue v2) {
        assert(lsp_get_tag(v1) == TAG_INT);
        assert(lsp_get_tag(v2) == TAG_INT);
        int64_t n1 = *lsp_get_number(v1);
        int64_t n2 = *lsp_get_number(v2);
        return lsp_new_number(n1 + n2);
}

int lsp_interpret(LspVm vm[static 1]) {
        LspState *state = vm->state;
        LspFunc *fn = &vm->state->funcs[0];
        size_t len = cvector_size(fn->instrs);
        while (vm->pc < len) {
                LspInstr i = fn->instrs[vm->pc];
                switch (lsp_get_opcode(i)) {
                        case OP_LDC:
                                vm->regs[lsp_get_arg1(i)] = lsp_new_number(
                                        state->ints[lsp_get_long_arg(i)]);
                                vm->pc++;
                                break;
                        case OP_ADD:
                                vm->regs[lsp_get_arg1(i)] = add(
                                        vm->regs[lsp_get_arg2(i)],
                                        vm->regs[lsp_get_arg3(i)]);
                                vm->pc++;
                                break;
                        case OP_LDF:
                                vm->regs[lsp_get_arg1(i)] = lsp_new_fn(lsp_get_arg2(i));
                                vm->pc++;
                                break;
                        default:
                                printf("Not implemented yet...\n");
                                exit(1);
                }
        }
        for (size_t i = 0; i < 256; ++i) {
                LspValue v = vm->regs[i];
                if (v) {
                        lsp_print_val(v);
                }
        }
        return 0;
}
