#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
        OP_LDC = 0,
        OP_ADD = 1,
} LspOpcode;


typedef uint32_t LspInstr;

LspOpcode lsp_get_opcode(LspInstr instr);

uint8_t lsp_get_arg1(LspInstr instr);

uint8_t lsp_get_arg2(LspInstr instr);

uint8_t lsp_get_arg3(LspInstr instr);

uint16_t lsp_get_long_arg(LspInstr instr);

LspInstr lsp_new_instr(LspOpcode opcode, uint8_t vals[static 3]);

LspInstr lsp_new_instr_l(LspOpcode opcode, uint8_t reg, uint16_t big);

const char *lsp_opcode_str(LspOpcode o);

void lsp_print_instr(LspInstr i);