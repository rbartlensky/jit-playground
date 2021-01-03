#include "gen.h"

#include <assert.h>
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
                  "                     | <comment> | <sexpr>  | <qexpr> ;   "
                  " lispy               : /^/ <expr>* /$/ ;                  ",
                  lang.number, lang.symbol, lang.string, lang.comment,
                  lang.sexpr, lang.expr, lang.lispy, NULL);

        return lang;
}

void lsp_cleanup_lang(LspLang lang[static 1]) {
        mpc_cleanup(7, lang->number, lang->symbol, lang->string, lang->comment,
                    lang->sexpr, lang->expr, lang->lispy);
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
        uint8_t reg = state->regs_in_use++;
        LspInstr instr = lsp_new_instr_l(
                OP_LDC,
                reg,
                (uint32_t)(cvector_size(state->ints) - 1));
        // save the instruction
        cvector_push_back(state->instrs, instr);
        *res = reg;
        return 0;
}

static int compile_sexpr(LspState state[static 1], mpc_ast_t *ast, uint8_t res[static 1]) {
        mpc_ast_print(ast);
        int sindex = mpc_ast_get_index(ast, "expr|symbol|regex");
        if (sindex != 1) {
                printf("Expected symbol at index 1.\n");
                return -1;
        }

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
                int ret = 0;
                if (strcmp(child->tag, "expr|number|regex") == 0) {
                        ret = compile_number(state, child, &out_regs[i-2]);
                } else if (strcmp(child->tag, "expr|sexpr|>") == 0) {
                        ret = compile_sexpr(state, child, &out_regs[i-2]);
                } else {
                        printf("Can only compile number arguments.\n");
                        ret = -1;
                }
                if (ret != 0) {
                        return -1;
                }
        }

        // create the instruction that adds the two operands together
        uint8_t out_reg = state->regs_in_use++;
        uint8_t args[3] = {out_reg, out_regs[0], out_regs[1]};
        cvector_push_back(state->instrs, lsp_new_instr(op, args));
        *res = out_reg;
        return 0;
}

LspState lsp_compile(mpc_ast_t *ast) {
        int index = mpc_ast_get_index(ast, "expr|sexpr|>");
        LspState s = {
                .ints = NULL,
                .instrs = NULL,
                .regs_in_use = 0,
        };
        uint8_t reg = 0;
        compile_sexpr(&s, ast->children[index], &reg);
	if (s.instrs) {
		for (size_t i = 0; i < cvector_size(s.instrs); ++i) {
			lsp_print_instr(s.instrs[i]);
		}
	}
        return s;
}

void lsp_cleanup_state(LspState s[static 1]) {
        cvector_free(s->ints);
        cvector_free(s->instrs);
}
