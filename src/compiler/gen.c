#include "gen.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

LspLang lsp_create_lang() {
        LspLang lang = {
                .number = mpc_new("number"),
                .symbol = mpc_new("symbol"),
                .string = mpc_new("string"),
                .comment = mpc_new("comment"),
                .sexpr = mpc_new("sexpr"),
                .expr = mpc_new("expr"),
                .lispy = mpc_new("lispy"),
        };

        mpca_lang(MPCA_LANG_PREDICTIVE,
                  " number  \"number\"  : /[0-9]+/ ;                         "
                  " symbol  \"symbol\"  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; "
                  " string  \"string\"  : /\"(\\\\.|[^\"])*\"/ ;             "
                  " comment             : /;[^\\r\\n]*/ ;                    "
                  " sexpr               : '(' <expr>* ')' ;                  "
                  " expr                : <number>  | <symbol> | <string>    "
                  "                     | <comment> | <sexpr> ;              "
                  " lispy               : /^/ <expr>* /$/ ;                  ",
                  lang.number, lang.symbol, lang.string, lang.comment,
                  lang.sexpr, lang.expr, lang.lispy, NULL);

        return lang;
}

void lsp_cleanup_lang(LspLang lang[static 1]) {
        mpc_cleanup(7, lang->number, lang->symbol, lang->string, lang->comment,
                    lang->sexpr, lang->expr, lang->lispy);
}

LspFunc lsp_new_func() {
        LspFunc ret = {
                .instrs = NULL,
                .num_of_params = 0,
                .regs_in_use = 0,
        };
        return ret;
}

void lsp_cleanup_func(LspFunc f[static 1]) {
        cvector_free(f->instrs);
}

static int compile_arithmetic_op(mpc_ast_t *ast, LspOpcode *opcode) {
        char *symbol = ast->contents;
        if (strcmp(symbol, "+") != 0) {
                printf("Can only compile additions.\n");
                return -1;
        }
        *opcode = OP_ADD;
        return 0;
}

static int compile_number(LspState state[static 1], mpc_ast_t *ast, uint8_t res[static 1]) {
        // convert the number into an int
        char *end;
        int64_t num = strtol(ast->contents, &end, 10);
        if (*end != '\0') {
                printf("Operand is not number: %s\n", ast->contents);
                return -1;
        }
        // save the constant
        cvector_push_back(state->ints, num);
        // load into the next available register the constant's index
        LspFunc *f = &state->funcs[state->curr_func];
        uint8_t reg = f->regs_in_use++;
        LspInstr instr = lsp_new_instr_l(
                OP_LDC,
                reg,
                (uint32_t)(cvector_size(state->ints) - 1));
        // save the instruction
        cvector_push_back(f->instrs, instr);
        *res = reg;
        return 0;
}

static bool is_arithmetic_op(char op[static 1]) {
        size_t len = strlen(op);
        if (len != 1) {
                return false;
        }
        switch (op[0]) {
                case '+':
                        return true;
                default:
                        return false;
        }
}

static int find_symbol(LspState state[static 1], char *sym, uint8_t res[static 1]) {
        LspFunc *f = &state->funcs[state->curr_func];
        for (size_t i = cvector_size(f->symbols) - 1; i < cvector_size(f->symbols); --i) {
                LspSymbol *s = &f->symbols[i];
                if (strcmp(s->name, sym) == 0) {
                        *res = s->reg;
                        return 0;
                }
        }
        return -1;
}

static int compile_sexpr(LspState state[static 1], mpc_ast_t *ast, uint8_t res[static 1]);

static int compile_expr(LspState state[static 1], mpc_ast_t *child, uint8_t res[static 1]) {
        if (strcmp(child->tag, "expr|number|regex") == 0) {
                return compile_number(state, child, res);
        } else if (strcmp(child->tag, "expr|sexpr|>") == 0) {
                return compile_sexpr(state, child, res);
        } else if (strcmp(child->tag, "expr|symbol|regex") == 0) {
                return find_symbol(state, child->contents, res);
        } else {
                printf("Can only compile number arguments.\n");
                return -1;
        }
        return -1;
}

static int compile_arithmetic_expr(LspState state[static 1],
                                   mpc_ast_t *ast,
                                   size_t sindex,
                                   uint8_t res[static 1]) {
        LspOpcode op;
        if (compile_arithmetic_op(ast->children[sindex], &op) != 0) {
                return -1;
        }

        if (ast->children_num != 5) {
                printf("Can only compile <sexpr> with two arguments, got %d arguments.\n",
                       ast->children_num - 3);
                return -1;
        }

        uint8_t out_regs[2] = {0, 0};
        for (int i = 2; i < ast->children_num - 1; ++i) {
                mpc_ast_t *child = ast->children[i];
                if (compile_expr(state, child, &out_regs[i-2]) != 0) {
                        return -1;
                }
        }

        // create the instruction that adds the two operands together
        LspFunc *f = &state->funcs[state->curr_func];
        uint8_t out_reg = f->regs_in_use++;
        uint8_t args[3] = {out_reg, out_regs[0], out_regs[1]};
        cvector_push_back(f->instrs, lsp_new_instr(op, args));
        *res = out_reg;
        return 0;
}

static int compile_parameters(LspFunc *f, mpc_ast_t *ast) {
        uint8_t regs = 0;
        for (int i = 1; i < ast->children_num - 1; ++i) {
                if (strcmp(ast->children[i]->tag, "expr|symbol|regex") == 0) {
                        LspSymbol sym = { .name = ast->children[i]->contents, .reg = regs++ };
                        cvector_push_back(f->symbols, sym);
                } else {
                        printf("Invalid parameter name.\n");
                        return -1;
                }
        }
        f->regs_in_use = regs;
        f->num_of_params = regs;
        return 0;
}

static int compile_defun(LspState state[static 1],
                         mpc_ast_t *ast,
                         size_t sindex,
                         uint8_t res[static 1]) {
        size_t saved_curr_func = state->curr_func++;
        size_t index = cvector_size(state->funcs);
        assert(index < 256);
        cvector_push_back(state->funcs, lsp_new_func());
        LspFunc *f = &state->funcs[state->curr_func];
        if (ast->children_num < 5) {
                printf("Bad function\n");
                return -1;
        }
        // sindex + 1 == function name
        // sindex + 2 == parameters list
        if (compile_parameters(f, ast->children[sindex + 2]) != 0) {
                return -1;
        }
        uint8_t last_res = 0;
        for (int i = sindex + 3; i < ast->children_num - 1; ++i) {
                compile_expr(state, ast->children[i], &last_res);
        }
        state->curr_func = saved_curr_func;
        LspFunc *curr_func = &state->funcs[saved_curr_func];
        uint8_t reg = curr_func->regs_in_use++;
        uint8_t args[3] = {reg, index, 0};
        LspInstr ldf = lsp_new_instr(OP_LDF, args);
        cvector_push_back(curr_func->instrs, ldf);
        printf("Saving symbol %s in func %ld!\n", ast->children[sindex + 1]->contents, saved_curr_func);
        LspSymbol sym = { .name = ast->children[sindex + 1]->contents, .reg = reg };
        cvector_push_back(curr_func->symbols, sym);
        *res = reg;
        return 0;
}

static int compile_call(LspState state[static 1],
                        mpc_ast_t *ast,
                        size_t sindex,
                        uint8_t res[static 1]) {
        // find the symbol that we are calling
        mpc_ast_t *symbol = ast->children[sindex];
        LspFunc *f = &state->funcs[state->curr_func];
        uint8_t sym_reg = f->regs_in_use++;
        if (find_symbol(state, symbol->contents, &sym_reg) != 0) {
                return -1;
        }
        {
                uint8_t args[3] = {f->regs_in_use - 1, sym_reg, 0};
                LspInstr instr = lsp_new_instr(OP_MOV, args);
                cvector_push_back(f->instrs, instr);
        }
        sym_reg = f->regs_in_use - 1;

        // compile each argument
        assert(ast->children_num - sindex <= 255);
        uint8_t total_args = ast->children_num - sindex - 2;
        uint8_t regs[256];
        for (int i = sindex + 1; i < ast->children_num - 1; ++i) {
                compile_expr(state, ast->children[i], &regs[i-sindex-1]);
        }

        // move each argument into a new register, such that all arguments are
        // in consecutive registers, and start at `sym_reg + 1`
        for (uint8_t i = 0; i < total_args; ++i) {
                if (sym_reg + i + 1 != regs[i]) {
                        uint8_t args[3] = {sym_reg + i + 1, regs[i], 0};
                        LspInstr instr = lsp_new_instr(OP_MOV, args);
                        cvector_push_back(f->instrs, instr);
                }
        }

        // prepare the call instruction
        uint8_t out_reg = f->regs_in_use++;
        uint8_t args[3] = {out_reg, sym_reg, sym_reg + total_args};
        LspInstr instr = lsp_new_instr(OP_CALL, args);
        cvector_push_back(f->instrs, instr);
        *res = out_reg;
        return 0;
}

static int compile_sexpr(LspState state[static 1], mpc_ast_t *ast, uint8_t res[static 1]) {
        mpc_ast_print(ast);
        int sindex = 1;
        if (strcmp(ast->children[sindex]->tag, "expr|symbol|regex") != 0) {
                mpc_state_t st = ast->children[sindex]->state;
                printf("Expected symbol at: %ld:%ld.\n", st.row + 1, st.col);
                return -1;
        }

        mpc_ast_t *symbol = ast->children[sindex];
        if (is_arithmetic_op(symbol->contents)) {
                return compile_arithmetic_expr(state, ast, sindex, res);
        } else if (strcmp(symbol->contents, "defun") == 0) {
                return compile_defun(state, ast, sindex, res);
        } else {
                return compile_call(state, ast, sindex, res);
        }
        return -1;
}

LspState lsp_compile(mpc_ast_t *ast) {
        LspState s = {
                .ints = NULL,
                .funcs = NULL,
        };
        uint8_t reg = 0;
        cvector_push_back(s.funcs, lsp_new_func());
        for (int i = 1; i < ast->children_num - 1; ++i) {
                if (compile_sexpr(&s, ast->children[i], &reg) != 0) {
                        break;
                }
        }
        for (size_t i = 0; i < cvector_size(s.funcs); ++i) {
                printf("Func %ld:\n", i);
                cvector_vector_type(LspInstr) instrs = s.funcs[i].instrs;
                if (instrs) {
                        for (size_t j = 0; j < cvector_size(instrs); ++j) {
                                lsp_print_instr(instrs[j]);
                        }
                }
        }
        return s;
}

void lsp_cleanup_state(LspState s[static 1]) {
        cvector_free(s->ints);
        for (size_t i = 0; i < cvector_size(s->funcs); ++i) {
                cvector_free(s->funcs[i].instrs);
        }
        cvector_free(s->funcs);
}
