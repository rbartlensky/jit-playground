#include "serde.h"

static uint8_t* encode_64_bits(void *p) {
        uint64_t v = *((uint64_t*) p);
        static uint8_t bytes[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 8; ++i) {
                bytes[i] = (uint8_t)(v & 0xff);
                v >>= 8;
        }
        return bytes;
}

static uint64_t decode_64_bits(uint8_t p[static 8]) {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
                v += ((uint64_t)(p[i])) << (i * 8);
        }
        return v;
}

static uint8_t* encode_32_bits(void *p) {
        uint32_t v = *((uint32_t*) p);
        static uint8_t bytes[4] = {0, 0, 0, 0};
        for (int i = 0; i <= 4; ++i) {
                bytes[i] = (uint8_t)(v & 0xff);
                v >>= 8;
        }
        return bytes;
}

static uint32_t decode_32_bits(uint8_t p[static 4]) {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
                v += ((uint32_t)(p[i])) << (i * 8);
        }
        return v;
}

static Vec encode_vec(uint8_t *v, size_t size_of_elem, uint8_t* encode_fn(void *)) {
        // the size of the vec + the size each element
        size_t size = cvector_size(v) * size_of_elem + sizeof(uint64_t);
        uint8_t *encoded = malloc(size);
        if (!encoded) {
                printf("Well, we are oom.\n");
                exit(1);
        }

        // first we encode the size
        uint64_t v_size = cvector_size(v);
        uint8_t *bytes = encode_64_bits(&v_size);
        memcpy(encoded, bytes, sizeof(uint64_t));

        // encode all elements
        for (size_t i = 0; i < cvector_size(v); ++i) {
                uint8_t *bytes = encode_fn(&v[i * size_of_elem]);
                memcpy(encoded + sizeof(uint64_t) + i * size_of_elem, bytes, 4);
        }

        Vec vec = { .size = size, .inner = encoded, };
        return vec;
}

Vec lsp_encode_state(const LspState state[static 1]) {
        Vec ints = encode_vec((uint8_t*)state->ints, sizeof(int64_t), encode_64_bits);
        Vec instrs = encode_vec((uint8_t*)state->instrs, sizeof(LspInstr), encode_32_bits);
        uint8_t *out = malloc(instrs.size + ints.size);
        if (!out) {
                printf("OOM!\n");
                exit(1);
        }
        memcpy(out, ints.inner, ints.size);
        memcpy(out + ints.size, instrs.inner, instrs.size);
        Vec ret = { .size = instrs.size + ints.size, .inner = out, };
        free(ints.inner);
        free(instrs.inner);
        return ret;
}

static size_t decode_vec_int64(Vec input, int64_t **output) {
        assert(input.size >= sizeof(uint64_t) && "Invalid buffer format!\n");

        uint64_t vec_size = decode_64_bits(input.inner);
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

        uint64_t vec_size = decode_64_bits(input.inner);
        assert(input.size >= sizeof(uint64_t) + vec_size * sizeof(LspInstr) && "Invalid buffer format!\n");

        size_t input_offset = sizeof(uint64_t);
        cvector_vector_type(LspInstr) res = NULL;
        for (uint64_t i = 0; i < vec_size; ++i) {
                cvector_push_back(res, decode_32_bits(input.inner + input_offset));
                input_offset += sizeof(LspInstr);
        }
        *output = res;
        return input_offset;
}

LspState lsp_decode_state(const Vec data) {
        cvector_vector_type(int64_t) ints = NULL;
        size_t offset = decode_vec_int64(data, &ints);
        Vec offset_data = {
                .inner = data.inner + offset,
                .size = data.size - offset,
        };
        cvector_vector_type(LspInstr) instrs = NULL;
        decode_vec_lsp_instr(offset_data, &instrs);
        LspState s = {
                .ints = ints,
                .instrs = instrs,
                .regs_in_use = 0,
        };
        return s;
}
