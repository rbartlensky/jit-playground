#include "utils.h"

#include <stdio.h>
#include <stdlib.h>

inline void* lsp_malloc(size_t s) {
        void *m = malloc(s);
        if (!m) {
                printf("OOM!\n");
                exit(-1);
        }
        return m;
}

inline void* lsp_realloc(void *ptr, size_t s) {
        void *m = realloc(ptr, s);
        if (!m) {
                printf("OOM!\n");
                exit(-1);
        }
        return m;
}
