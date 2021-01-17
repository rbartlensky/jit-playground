#pragma once

#define CVECTOR_LOGARITHMIC_GROWTH

#include "cvector.h"
#include <mpc.h>

#include "opcodes.h"

typedef struct LspLang {
        mpc_parser_t* number;
        mpc_parser_t* symbol;
        mpc_parser_t* string;
        mpc_parser_t* comment;
        mpc_parser_t* sexpr;
        mpc_parser_t* expr;
        mpc_parser_t* lispy;
} LspLang;

typedef struct LspSymbol {
        char *name;
        uint8_t reg;
} LspSymbol;

/** The state of a function. */
typedef struct LspFunc {
        cvector_vector_type(LspSymbol) symbols;
        cvector_vector_type(LspInstr) instrs;
        uint8_t num_of_params;
        uint8_t regs_in_use;
} LspFunc;

/** The compiler's state. */
typedef struct LspState {
        /* The constant table. */
        cvector_vector_type(int64_t) ints;
        /* Compiled functions: 0 is the "main" function. */
        cvector_vector_type(LspFunc) funcs;
        size_t curr_func;
} LspState;

/** A vector (pointer to an array + its size). */
typedef struct Vec {
        size_t size;
        uint8_t *inner;
} Vec;

LspLang lsp_create_lang();

void lsp_cleanup_lang(LspLang lang[static 1]);

/** Compiles the parsed input. */
LspState lsp_compile(mpc_ast_t *ast);

void lsp_cleanup_state(LspState state[static 1]);

LspFunc lsp_new_func();

void lsp_cleanup_func(LspFunc f[static 1]);
