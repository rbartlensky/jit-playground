#include <vm.h>
#include <compiler/gen.h>

int main(int argc, char **argv) {
        LspLang lang = lsp_create_lang();
        mpc_result_t r;
        if (argc > 1) {
                if (mpc_parse_contents(argv[1], lang.lispy, &r)) {
                        LspState s = lsp_compile(r.output);
                        LspVm vm = lsp_new_vm(&s);
                        lsp_interpret(&vm);
                        lsp_cleanup_vm(&vm);
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
