#include "jit.h"

LspJit lsp_jit_new(LspState s[static 1]) {
        LLVMLinkInMCJIT();
        LLVMInitializeNativeTarget();
        LLVMInitializeNativeAsmPrinter();

        LLVMModuleRef mod = LLVMModuleCreateWithName("compiled_func");
        LLVMExecutionEngineRef engine;
        char *error = NULL;
        if (LLVMCreateExecutionEngineForModule(&engine, mod, &error)) {
                printf("Failed to create execution engine: %s", error);
                exit(1);
        }

        LspVm vm = lsp_new_vm(s);
        LLVMValueRef *compiled_funcs = NULL;
        for (size_t i = 0; i < cvector_size(s->funcs); ++i) {
                cvector_push_back(compiled_funcs, NULL);
        }
        LspJit jit = {
                .traces = lsp_trace_map_new(),
                .open_traces = NULL,
                .vm = vm,
                .module = mod,
                .engine = engine,
                .compiled_funcs = compiled_funcs,
        };
        return jit;
}

void lsp_jit_free(LspJit self[static 1]) {
        lsp_trace_map_free(&self->traces);
        cvector_free(self->open_traces);
        cvector_free(self->compiled_funcs);
        lsp_cleanup_vm(&self->vm);
        LLVMDisposeExecutionEngine(self->engine);
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

int lsp_dispatch() {
        printf("DISPATCHED!\n");
        return 1;
}

static LLVMValueRef const_int(int64_t i) {
        return LLVMConstInt(LLVMInt64Type(), i, 0);
}

static void compile_trace(LspJit self[static 1], size_t f, TraceList trace[static 1]) {
        LspFunc *func = &self->vm.state->funcs[f];
        // int64_t f(int64_t[num_of_params])
        LLVMTypeRef param_type[1] = { LLVMArrayType(LLVMInt64Type(), func->num_of_params) };
        LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt64Type(), param_type, 1, 0);
        LLVMValueRef llvm_fn = LLVMAddFunction(self->module, func->name, ret_type);

        LLVMTypeRef ret_type2 = LLVMFunctionType(LLVMInt64Type(), NULL, 0, 0);
        LLVMAddFunction(self->module, "lsp_dispatch", ret_type2);
        LLVMTypeRef dispatch_fn_ptr = LLVMPointerType(ret_type2, 0);
        LLVMValueRef dispatch = LLVMConstInt(LLVMInt64Type(), (uint64_t)lsp_dispatch, 0);


        LLVMBasicBlockRef entry = LLVMAppendBasicBlock(llvm_fn, "entry");
        LLVMBuilderRef builder = LLVMCreateBuilder();
        LLVMPositionBuilderAtEnd(builder, entry);
        LLVMValueRef rf = LLVMBuildIntToPtr(builder, dispatch, dispatch_fn_ptr, "");


        LLVMValueRef params = LLVMGetParam(llvm_fn, 0);
        LLVMValueRef regs[256];
        for (uint8_t i = 0; i < func->num_of_params; ++i) {
                LLVMValueRef is[1] = { const_int(i) };
                LLVMValueRef gep = LLVMBuildInBoundsGEP(builder, params, is, 1, "");
                regs[i] = LLVMBuildLoad(builder, gep, "");
        }

        // skip first instruction always
        TraceNode *n = trace->head->children[0];
        while (n) {
                LspInstr i = n->instr;
                uint8_t r1 = lsp_get_arg1(i);
                switch (lsp_get_opcode(i)) {
                case OP_LDC: {
                        regs[r1] = const_int(self->vm.state->ints[lsp_get_arg2(i)]);
                } break;
                case OP_ADD: {
                        uint8_t r2 = lsp_get_arg2(i), r3 = lsp_get_arg3(i);
                        regs[r1] = LLVMBuildAdd(builder, regs[r2], regs[r3], "");
                } break;
                case OP_SUB: {
                        uint8_t r2 = lsp_get_arg2(i), r3 = lsp_get_arg3(i);
                        regs[r1] = LLVMBuildSub(builder, regs[r2], regs[r3], "");
                } break;
                case OP_RET:
                        LLVMBuildRet(builder, regs[r1]);
                        break;
                case OP_MOV: {
                        regs[r1] = regs[lsp_get_arg2(i)];
                } break;
                case OP_EQ: {
                        uint8_t r2 = lsp_get_arg2(i), r3 = lsp_get_arg3(i);
                        regs[r1] = LLVMBuildICmp(builder, LLVMIntEQ, regs[r2], regs[r3], "");
                } break;
                case OP_CALL: {
                        regs[r1] = LLVMBuildCall(builder, rf, NULL, 0, "");
                } break;
                case OP_LDF: {
                        regs[r1] = const_int(lsp_get_arg2(i));
                } break;
                case OP_JMP:
                case OP_TEST:
                        break;
                }
                n = n->children[0];
        }
        self->compiled_funcs[f] = llvm_fn;
        LLVMDisposeBuilder(builder);
        LLVMDumpModule(self->module);
}

void lsp_jit_trace_end(LspJit self[static 1], size_t func) {
        size_t last = cvector_size(self->open_traces);
        if (last == 0 || func == 0) {
                return;
        }
        TraceList list = self->open_traces[last - 1];
        cvector_pop_back(self->open_traces);
        // list deallocation is handled by the map
        bool is_hot = lsp_trace_map_insert(&self->traces, func, &list);
        if (is_hot && !self->compiled_funcs[func]) {
                compile_trace(self, func, &list);
        }
        lsp_trace_list_free(&list);
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

        LLVMValueRef func = jit->compiled_funcs[fn_index];
        if (func) {
                int64_t *params = lsp_malloc(sizeof(int64_t) * fn->num_of_params);
                for (uint8_t r = r2 + 1; r < r3; ++r) {
                        params[i] = *lsp_get_number(vm->regs[r]);
                }
                int64_t f_addr = LLVMGetFunctionAddress(jit->engine, fn->name);
                int64_t (*f)(int64_t*) = (int64_t (*)(int64_t*))f_addr;
                int64_t ret = (f)(params);
                LspValue new_val = lsp_new_number(ret);
                lsp_replace_val(&vm->regs[r1], &new_val);
                free(params);
                printf("Compiled func returned: %ld in %d\n", ret, r1);
                vm->pc++;
                return 0;
        }

        lsp_jit_trace_start(jit);
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
