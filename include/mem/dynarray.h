/* SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD */

#ifndef UCLP_MEM_DYNARRAY_H
#define UCLP_MEM_DYNARRAY_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t capacity;
    size_t size;
    uint8_t *mem [[clang::counted_by(size)]];
} ByteBuffer;

inline size_t new_capacity(size_t old_cap) {
    return old_cap ? old_cap * 2 : 8;
}

void append_bytes(ByteBuffer *buf, size_t size, uint8_t data[size]);

#undef DYN_ARRAY_TYPEDEF

#endif /* UCLP_MEM_DYNARRAY_H */
