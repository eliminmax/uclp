/* SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD */

#ifndef UCLP_MEM_DYNARRAY_H
#define UCLP_MEM_DYNARRAY_H
#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

#include "typenames.h"

typedef struct {
    size_t capacity;
    size_t size;
    uchar *mem [[clang::counted_by(size)]];
} CharBuffer;

inline size_t new_capacity(size_t old_cap) {
    return old_cap >= 8 ? old_cap * 2 : 8;
}

void append_chars(CharBuffer *buf, size_t size, const uchar data[size]);

#endif /* UCLP_MEM_DYNARRAY_H */
