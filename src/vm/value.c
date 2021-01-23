#include "value.h"

#include <stdio.h>
#include <stdlib.h>

#define LSP_TAG_MASK 0xfffffffffffffff0

static void* lsp_malloc(size_t s) {
        void *m = malloc(s);
        if (!m) {
                printf("OOM!\n");
                exit(-1);
        }
        return m;
}

LspValue lsp_new_number(int64_t n) {
        int64_t *n_ptr = lsp_malloc(sizeof(n));
        *n_ptr = n;
        return ((uintptr_t)n_ptr & LSP_TAG_MASK) + TAG_INT;
}

LspValue lsp_new_fn(uint8_t fn) {
        uint16_t fn2 = ((uint16_t) fn) << 4;
        return ((uintptr_t)fn2 & LSP_TAG_MASK) + TAG_FN;
}

LspTag lsp_get_tag(LspValue v) {
        return v & 0xf;
}

int64_t* lsp_get_number(LspValue v) {
        return (int64_t*)(v | TAG_INT);
}

uint8_t lsp_get_fn(LspValue v) {
        return (uint8_t)((v | TAG_FN) >> 4);
}

void lsp_print_val(LspValue v) {
        switch (lsp_get_tag(v)) {
                case TAG_INT:
                        printf("Num: %ld\n", *lsp_get_number(v));
                        break;
                case TAG_FN:
                        printf("Fn: %d\n", lsp_get_fn(v));
                        break;
        }
}
