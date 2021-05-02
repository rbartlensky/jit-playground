#include "jit.h"

LspJit lsp_jit_new(LspState s[static 1]) {
        LspJit jit = {
                .traces = lsp_trace_map_new(),
                .open_traces = NULL,
                .vm = lsp_new_vm(s),
        };
        return jit;
}

void lsp_jit_free(LspJit self[static 1]) {
        lsp_trace_map_free(&self->traces);
        cvector_free(self->open_traces);
        lsp_cleanup_vm(&self->vm);
}

void lsp_jit_trace_start(LspJit self[static 1]) {
        TraceNode empty = lsp_trace_node_new(0, NODE_MD_NONE);
        cvector_push_back(self->open_traces, lsp_trace_list_new(empty));
}

void lsp_jit_record(LspJit self[static 1], LspInstr i) {
        // skip jmp instructions for now
        uint8_t opcode = lsp_get_opcode(i);
        if (opcode == OP_JMP) {
                return;
        }
        size_t last = cvector_size(self->open_traces);
        if (last == 0) {
                return;
        }
        NodeMetadata md = NODE_MD_NONE;
        if (opcode == OP_TEST) {
                LspValue val = self->vm.regs[lsp_get_arg1(i) + self->vm.regs_start];
                md = lsp_val_to_bool(val) == true ? NODE_MD_TRUE : NODE_MD_FALSE;
        }
        lsp_trace_list_add(&self->open_traces[last - 1], lsp_trace_node_new(i, md));
}

void lsp_jit_trace_end(LspJit self[static 1], size_t func) {
        size_t last = cvector_size(self->open_traces);
        if (last == 0 || func == 0) {
                return;
        }
        TraceList list = self->open_traces[last - 1];
        cvector_pop_back(self->open_traces);
        // list deallocation is handled by the map
        lsp_trace_map_insert(&self->traces, func, &list);
}

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
        for (size_t i = 0; i < cvector_size(vm->regs); ++i) {
                lsp_free_val(vm->regs[i]);
        }
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

inline static int interpret_call(LspJit jit[static 1], LspInstr i) {
        LspVm *vm = &jit->vm;
        lsp_jit_trace_start(jit);

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
                LspValue v = lsp_copy_val(&vm->regs[i]);
                lsp_replace_val(&vm->regs[j], &v);
        }
        int ret = lsp_interpret(jit);

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

        lsp_jit_trace_end(jit, fn_index);
        return ret;
}

int lsp_interpret(LspJit self[static 1]) {
        LspVm *vm = &self->vm;
        LspState *state = vm->state;
        LspFunc *fn = &vm->state->funcs[vm->curr_fn];
        size_t len = cvector_size(fn->instrs);
        while (vm->pc < len) {
                LspInstr i = fn->instrs[vm->pc];
                lsp_jit_record(self, i);
                switch (lsp_get_opcode(i)) {
                case OP_LDC: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        LspValue num = lsp_new_number(
                                state->ints[lsp_get_long_arg(i)]);
                        lsp_replace_val(&vm->regs[r1], &num);
                        vm->pc++;
                }
                        break;
                case OP_ADD: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        LspValue add_v = add(vm->regs[r2], vm->regs[r3]);
                        lsp_replace_val(&vm->regs[r1], &add_v);
                        vm->pc++;
                }
                        break;
                case OP_SUB: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        LspValue sub_v = sub(vm->regs[r2], vm->regs[r3]);
                        lsp_replace_val(&vm->regs[r1], &sub_v);
                        vm->pc++;
                }
                        break;
                case OP_EQ: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        uint8_t r3 = lsp_get_arg3(i) + vm->regs_start;
                        LspValue eq_v = eq(vm->regs[r2], vm->regs[r3]);
                        lsp_replace_val(&vm->regs[r1], &eq_v);
                        vm->pc++;
                }
                        break;
                case OP_LDF: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        LspValue fun = lsp_new_fn(lsp_get_arg2(i));
                        lsp_replace_val(&vm->regs[r1], &fun);
                        vm->pc++;
                }
                        break;
                case OP_MOV: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        uint8_t r2 = lsp_get_arg2(i) + vm->regs_start;
                        lsp_replace_val(&vm->regs[r1], &vm->regs[r2]);
                        vm->pc++;
                }
                        break;
                case OP_CALL:
                        interpret_call(self, i);
                        break;
                case OP_TEST: {
                        uint8_t r1 = lsp_get_arg1(i) + vm->regs_start;
                        LspValue v1 = vm->regs[r1];
                        if (lsp_val_to_bool(v1)) {
                                vm->pc++;
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
