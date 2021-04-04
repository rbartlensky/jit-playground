#include "value.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

#define LSP_TAG_MASK 0xfffffffffffffff0

LspValue lsp_new_number(int64_t n) {
        int64_t *n_ptr = lsp_malloc(sizeof(n));
        *n_ptr = n;
        return ((uintptr_t)n_ptr & LSP_TAG_MASK) + TAG_INT;
}

inline LspValue lsp_new_fn(uint8_t fn) {
        uint16_t fn2 = ((uint16_t) fn) << 4;
        return ((uintptr_t)fn2 & LSP_TAG_MASK) + TAG_FN;
}

inline LspTag lsp_get_tag(LspValue v) {
        return v & 0xf;
}

inline int64_t* lsp_get_number(LspValue v) {
        return (int64_t*)(v | TAG_INT);
}

inline uint8_t lsp_get_fn(LspValue v) {
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

void lsp_free_val(LspValue v) {
        switch (lsp_get_tag(v)) {
                case TAG_INT:
                        free(lsp_get_number(v));
                        break;
                default:
                        break;
        }
}

void lsp_replace_val(LspValue *self, LspValue with) {
        lsp_free_val(*self);
        *self = with;
}

void lsp_exchange_val(LspValue *self, LspValue *with) {
        LspValue tmp = *self;
        *self = *with;
        *with = tmp;
}

LspValue lsp_copy_val(LspValue *self) {
        switch (lsp_get_tag(*self)) {
                case TAG_INT:
                        return lsp_new_number(*lsp_get_number(*self));
                default:
                        return *self;
        }
}
