#pragma once

#include "compiler/gen.h"
#include "traces.h"
#include "value.h"

#include <cvector.h>
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

typedef struct LspVm {
        size_t regs_start;
        cvector_vector_type(LspValue) regs;
        uint64_t pc;
        LspState *state;
        size_t curr_fn;
} LspVm;

LspVm lsp_new_vm(LspState state[static 1]);

void lsp_cleanup_vm(LspVm vm[static 1]);

typedef struct LspJit {
        TraceMap traces;
        cvector_vector_type(TraceList) open_traces;
        LspVm vm;
        LLVMModuleRef module;
        LLVMExecutionEngineRef engine;
        cvector_vector_type(LLVMValueRef) compiled_funcs;
} LspJit;

LspJit lsp_jit_new(LspState state[static 1]);

void lsp_jit_free(LspJit self[static 1]);

void lsp_jit_trace_start(LspJit self[static 1]);

void lsp_jit_record(LspJit self[static 1], LspInstr i);

void lsp_jit_trace_end(LspJit self[static 1], size_t func);

int lsp_interpret(LspJit self[static 1]);

int lsp_dispatch();
