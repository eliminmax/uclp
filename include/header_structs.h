/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_HEADER_STRUCTS
#define UCLP_HEADER_STRUCTS

#include <stdint.h>

#define UCF_MAGIC "\xf8UCF"
#define UCF_VERSION_DEV 0

typedef struct Header {
    // magic bytes identifying the file type - must be set to UCF_MAGIC
    char magic[4];
    // version number
    uint8_t version;
    // number of FFI `dlopen` handles
    uint8_t num_ffi_handles;
    // number of FFI functions
    uint16_t num_ffi_funcs;
    // the size of the FFI segment
    uint64_t ffi_size;
    // the size of the variable segment
    uint64_t var_size;
    // the size of the code segment
    uint64_t code_size;
} Header;

static_assert(sizeof(Header) == 32);

typedef struct FFIFunc {
    // The index within the array of dynamic library handles
    uint8_t handle_index;
    // The NULL-terminated symbol to load from the handle
    char symbol[];
} FFIFunc;

#endif
