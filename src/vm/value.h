
#pragma once

#include <stdint.h>

typedef enum LspTag {
        TAG_INT = 0,
        TAG_FN = 1,
} LspTag;

typedef uintptr_t LspValue;

LspValue lsp_new_number(int64_t n);

LspValue lsp_new_fn(uint8_t fn);

LspTag lsp_get_tag(LspValue v);

int64_t* lsp_get_number(LspValue v);

void lsp_print_val(LspValue v);
