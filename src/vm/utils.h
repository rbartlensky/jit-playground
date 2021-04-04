#pragma once

#include <stddef.h>

void* lsp_malloc(size_t s);

void* lsp_realloc(void *ptr, size_t s);
