/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

typedef struct [[gnu::packed]] [[gnu::aligned(8)]] {
    char magic[4];
    uint8_t version;
    uint8_t num_ffi_handles;
    uint16_t num_ffi_funcs;
    uint64_t ffi_size;
    uint64_t var_size;
    uint64_t code_size;
} Header;
