#include "serde.h"

static Vec encode_64_bits(void *p) {
        uint64_t v = *((uint64_t*) p);
        static uint8_t bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 8; ++i) {
                bytes[i] = (uint8_t)(v & 0xff);
                v >>= 8;
        }
        Vec ret = { .inner = bytes, .size = 8, };
        return ret;
}

static uint64_t decode_64_bits(uint8_t p[static 8]) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
                v += ((uint64_t)(p[i])) << (i * 8);
        }
        return v;
}

static Vec encode_32_bits(void *p) {
        uint32_t v = *((uint32_t*) p);
        static uint8_t bytes[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
                bytes[i] = (uint8_t)(v & 0xff);
                v >>= 8;
        }
        Vec ret = { .inner = bytes, .size = 4, };
        return ret;
}

static uint32_t decode_32_bits(uint8_t p[static 4]) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
                v += ((uint32_t)(p[i])) << (i * 8);
        }
        return v;
}

static Vec encode_vec(uint8_t *v, size_t size_of_elem, Vec encode_fn(void *)) {
        // the size of the vec + the size each element
        size_t size = cvector_size(v) * size_of_elem + sizeof(uint64_t);
        uint8_t *encoded = malloc(size);
        if (!encoded) {
                printf("Well, we are oom.\n");
                exit(1);
        }

        // first we encode the size
        size_t v_size = cvector_size(v);
        Vec bytes = encode_64_bits(&v_size);
        memcpy(encoded, bytes.inner, bytes.size);

        // encode all elements
        for (size_t i = 0; i < cvector_size(v); ++i) {
                Vec bytes = encode_fn(&v[i * size_of_elem]);
                memcpy(encoded + sizeof(uint64_t) + i * size_of_elem, bytes.inner, bytes.size);
        }

        Vec vec = { .size = size, .inner = encoded, };
        return vec;
}

static Vec encode_lsp_func(void *v) {
        const LspFunc *f = v;
        Vec instrs = encode_vec((uint8_t*)f->instrs, sizeof(LspInstr), encode_32_bits);
        // extra byte for number of params
        uint8_t *out = malloc(instrs.size + 1);
        if (!out) {
                printf("OOM!\n");
                exit(1);
        }
        memcpy(out, instrs.inner, instrs.size);
        out[instrs.size] = f->num_of_params;
        Vec ret = { .size = instrs.size + 1, .inner = out, };
        free(instrs.inner);
        return ret;
}

Vec lsp_encode_state(const LspState state[static 1]) {
        Vec ints = encode_vec((uint8_t*)state->ints, sizeof(int64_t), encode_64_bits);
        Vec funcs = encode_vec((uint8_t*)state->funcs, sizeof(LspFunc), encode_lsp_func);
        uint8_t *out = malloc(ints.size + funcs.size);
        if (!out) {
                printf("OOM!\n");
                exit(1);
        }
        memcpy(out, ints.inner, ints.size);
        memcpy(out + ints.size, funcs.inner, funcs.size);
        Vec ret = { .size = ints.size + funcs.size, .inner = out, };
        free(ints.inner);
        free(funcs.inner);
        return ret;
}

static size_t decode_vec_int64(Vec input, int64_t **output) {
        assert(input.size >= sizeof(uint64_t) && "Invalid buffer format!\n");

        size_t vec_size = decode_64_bits(input.inner);
        assert(input.size >= sizeof(uint64_t) + vec_size * sizeof(int64_t) && "Invalid buffer format!\n");

        size_t input_offset = sizeof(uint64_t);
        cvector_vector_type(int64_t) res = NULL;
        for (uint64_t i = 0; i < vec_size; ++i) {
                cvector_push_back(res, decode_64_bits(input.inner + input_offset));
                input_offset += sizeof(int64_t);
        }
        *output = res;
        return input_offset;
}

static size_t decode_vec_lsp_instr(Vec input, LspInstr **output) {
        assert(input.size >= sizeof(uint64_t) && "Invalid buffer format!\n");

        size_t vec_size = decode_64_bits(input.inner);
        assert(input.size >= sizeof(uint64_t) + vec_size * sizeof(LspInstr) && "Invalid buffer format!\n");

        size_t input_offset = sizeof(uint64_t);
        cvector_vector_type(LspInstr) res = NULL;
        for (size_t i = 0; i < vec_size; ++i) {
                cvector_push_back(res, decode_32_bits(input.inner + input_offset));
                input_offset += sizeof(LspInstr);
        }
        *output = res;
        return input_offset;
}

static size_t decode_vec_lsp_func(Vec input, LspFunc **output) {
        assert(input.size >= sizeof(uint64_t) && "Invalid buffer format!\n");

        size_t vec_size = decode_64_bits(input.inner);
        assert(input.size == sizeof(uint64_t) + sizeof(LspFunc) * vec_size &&
               "Invalid buffer format!\n");

        size_t offset = 0;
        cvector_vector_type(LspFunc) res = NULL;
        for (size_t i = 0; i < vec_size; ++i) {
                cvector_vector_type(LspInstr) instrs = NULL;
                Vec v = {
                        .inner = input.inner + offset,
                        .size = input.size - offset,
                };
                offset += decode_vec_lsp_instr(v, &instrs);
                LspFunc f = {
                        .instrs = instrs,
                        .num_of_params = input.inner[offset],
                };
                cvector_push_back(res, f);
        }
        *output = res;
        return offset;
}

LspState lsp_decode_state(const Vec data) {
        cvector_vector_type(int64_t) ints = NULL;
        size_t offset = decode_vec_int64(data, &ints);
        Vec offset_data = {
                .inner = data.inner + offset,
                .size = data.size - offset,
        };
        cvector_vector_type(LspFunc) funcs = NULL;
        decode_vec_lsp_func(offset_data, &funcs);
        LspState s = {
                .ints = ints,
                .funcs = funcs,
                .regs_in_use = 0,
        };
        return s;
}
