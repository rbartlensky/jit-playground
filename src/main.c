#include <vm/jit.h>
#include <compiler/gen.h>

int main(int argc, char **argv) {
        LspLang lang = lsp_create_lang();
        mpc_result_t r;
        if (argc > 1) {
                if (mpc_parse_contents(argv[1], lang.lispy, &r)) {
                        LspState s = lsp_compile(r.output);
                        LspJit jit = lsp_jit_new(&s);
                        LspVm *vm = &jit.vm;
                        lsp_interpret(&jit);
                        for (size_t i = 0; i < cvector_size(vm->regs); ++i) {
                                LspValue v = vm->regs[i];
                                if (v) {
                                        printf("Reg[%ld]: ", i);
                                        lsp_print_val(v);
                                }
                        }
                        lsp_jit_free(&jit);
                        lsp_cleanup_state(&s);
                        mpc_ast_delete(r.output);
                } else {
                        mpc_err_print(r.error);
                        mpc_err_delete(r.error);
                }
        }
        lsp_cleanup_lang(&lang);
        return 0;
}
