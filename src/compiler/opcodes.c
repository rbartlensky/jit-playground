#include "opcodes.h"

LspOpcode lsp_get_opcode(LspInstr instr) {
        return instr >> 24;
}

uint8_t lsp_get_arg1(LspInstr instr) {
        return (instr >> 16) & 0xff;
}

uint8_t lsp_get_arg2(LspInstr instr) {
        return (instr >> 8) & 0xff;
}

uint8_t lsp_get_arg3(LspInstr instr) {
        return instr & 0xff;
}

uint16_t lsp_get_long_arg(LspInstr instr) {
        return instr & 0xffff;
}

LspInstr lsp_new_instr(LspOpcode opcode, uint8_t vals[static 3]) {
        LspInstr instr = vals[2];
        instr += ((uint32_t) opcode << 24) +
                ((uint32_t) vals[0] << 16) +
                ((uint32_t) vals[1] << 8);
        return instr;
}

LspInstr lsp_new_instr_l(LspOpcode opcode, uint8_t reg, uint16_t big) {
        LspInstr instr = big;
        instr += ((uint32_t) opcode << 24) +
                ((uint32_t) reg << 16);
        return instr;
}

const char *lsp_opcode_str(LspOpcode o) {
        switch(o) {
                case OP_LDC:
                        return "LDC";
                case OP_ADD:
                        return "ADD";
                case OP_LDF:
                        return "LDF";
                default:
                        printf("UNKNOWN OPCODE %d!\n", o);
                        exit(1);
        }
}

void lsp_print_instr(LspInstr i) {
        LspOpcode o = lsp_get_opcode(i);
        printf("%s: %d %d %d\n", lsp_opcode_str(o),
               lsp_get_arg1(i), lsp_get_arg2(i), lsp_get_arg3(i));
}
