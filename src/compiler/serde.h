#pragma once

#include "gen.h"

/** Encodes the compiler's state into bytes that can be written to disk. */
Vec lsp_encode_state(LspState const state[static 1]);

/** Decodes the compiler's state from bytes. This expects the whole data to be
loaded in memory. */
LspState lsp_decode_state(const Vec data);
