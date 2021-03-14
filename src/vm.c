#include "vm.h"

LspVm lsp_new_vm(LspState state[static 1]) {
        cvector_vector_type(LspValue) regs = NULL;
        for (size_t i = 0; i < 256; ++i) {
                cvector_push_back(regs, 0);
        }
        LspVm vm = {
                .regs_start = 0,
                .regs = regs,
                .pc = 0,
                .state = state,
                .curr_fn = 0,
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

static LspValue sub(LspValue v1, LspValue v2) {
        assert(lsp_get_tag(v1) == TAG_INT);
        assert(lsp_get_tag(v2) == TAG_INT);
        int64_t n1 = *lsp_get_number(v1);
        int64_t n2 = *lsp_get_number(v2);
        return lsp_new_number(n1 - n2);
}

static LspValue eq(LspValue v1, LspValue v2) {
        LspTag t1 = lsp_get_tag(v1);
        assert(t1 == lsp_get_tag(v2));
        if (t1 == TAG_INT) {
                int64_t n1 = *lsp_get_number(v1);
                int64_t n2 = *lsp_get_number(v2);
                return lsp_new_number(n1 == n2);
        }
        return lsp_new_number(v1 == v2);
}

inline static size_t create_stack_frame(LspVm vm[static 1], size_t end, size_t new_len) {
        if (new_len == 0) {
                printf("Bad new_len\n");
                exit(1);
        }
        size_t regs_len = cvector_size(vm->regs);
        size_t new_regs = 0;
        if (end >= regs_len) {
                new_regs = new_len + end - regs_len;
        } else {
                size_t diff = regs_len - end;
                if (new_len > diff) {
                        new_regs = new_len - diff;
                }
        }
        for (size_t i = 0; i < new_regs; ++i) {
                cvector_push_back(vm->regs, 0);
        }
        return end;
}

inline static int interpret_call(LspVm vm[static 1], LspInstr i) {
        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
        LspValue v2 = vm->regs[r2];
        if (lsp_get_tag(v2) != TAG_FN) {
                printf("Not a function.\n");
                exit(1);
        }

        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
        uint8_t fn_index = lsp_get_fn(v2);
        if (fn_index > cvector_size(vm->state->funcs)) {
                printf("Function index oob.\n");
                exit(1);
        }
        LspFunc *fn = &vm->state->funcs[fn_index];
        if (r3 <= r2 || r3 - r2 - 1 > fn->num_of_params) {
                printf("Function was expecting %d args, found %d.\n",
                       fn->num_of_params,
                       r3 - r2- 1);
                exit(1);
        }

        // make sure we can accommodate the new registers
        size_t top = create_stack_frame(
                vm,
                vm->regs_start + vm->state->funcs[vm->curr_fn].regs_in_use,
                fn->regs_in_use);

        // save the old state
        size_t old_pc = vm->pc;
        size_t old_fn = vm->curr_fn;
        size_t old_regs_start = vm->regs_start;

        vm->pc = 0;
        vm->curr_fn = fn_index;
        vm->regs_start = top;

        // copy all parameters to the new stack frame
        for (size_t i = r2 + 1, j = top; i <= r3; ++i, ++j) {
                vm->regs[j] = lsp_copy_val(&vm->regs[i]);
        }
        int ret = lsp_interpret(vm);

        // save ret value
        size_t last_instr = cvector_size(fn->instrs) - 1;
        LspInstr ret_instr = fn->instrs[last_instr];
        if (lsp_get_opcode(ret_instr) != OP_RET) {
                printf("Bytecode of function didn't end in 'ret'.\n");
                return -1;
        }
        uint8_t r_ret = lsp_get_arg1(ret_instr) + vm->regs_start;
        lsp_exchange_val(&vm->regs[r1], &vm->regs[r_ret]);

        // restore old state
        vm->pc = ++old_pc;
        vm->curr_fn = old_fn;
        vm->regs_start = old_regs_start;
        return ret;
}

int lsp_interpret(LspVm vm[static 1]) {
        LspState *state = vm->state;
        LspFunc *fn = &vm->state->funcs[vm->curr_fn];
        size_t len = cvector_size(fn->instrs);
        while (vm->pc < len) {
                LspInstr i = fn->instrs[vm->pc];
                switch (lsp_get_opcode(i)) {
                case OP_LDC: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        LspValue num = lsp_new_number(
                                state->ints[lsp_get_long_arg(i)]);
                        lsp_replace_val(&vm->regs[r1], num);
                        vm->pc++;
                }
                        break;
                case OP_ADD: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1],
                                        add(vm->regs[r2], vm->regs[r3]));
                        vm->pc++;
                }
                        break;
                case OP_SUB: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1],
                                        sub(vm->regs[r2], vm->regs[r3]));
                        vm->pc++;
                }
                        break;
                case OP_EQ: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1],
                                        eq(vm->regs[r2], vm->regs[r3]));
                        vm->pc++;
                }
                        break;
                case OP_LDF: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1],
                                        lsp_new_fn(lsp_get_arg2(i)));
                        vm->pc++;
                }
                        break;
                case OP_MOV: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1], vm->regs[r2]);
                        vm->pc++;
                }
                        break;
                case OP_CALL:
                        interpret_call(vm, i);
                        break;
                case OP_TEST: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        LspValue v1 = vm->regs[r1];
                        LspTag t1 = lsp_get_tag(v1);
                        switch (t1) {
                        case TAG_INT: {
                                int64_t *n = lsp_get_number(v1);
                                if (*n != 0) {
                                        vm->pc++;
                                }
                        }
                                break;
                        case TAG_FN:
                                vm->pc++;
                                break;
                        }
                        vm->pc++;
                }
                        break;
                case OP_JMP:
                        vm->pc += lsp_get_long_arg(i);
                        break;
                case OP_RET:
                        // most of this is handled by call
                        vm->pc++;
                        break;
                default:
                        printf("Not implemented yet...\n");
                        exit(1);
                }
        }
        return 0;
}
