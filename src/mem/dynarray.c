/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#include <stddef.h>
#include <string.h>

#include "mem/checked.h"
#include "mem/dynarray.h"
#include "typenames.h"

extern inline size_t new_capacity(size_t old_cap);

void append_chars(CharBuffer *buf, size_t size, const uchar data[size]) {
    if (buf->capacity - buf->size < size) {
        do {
            buf->capacity = new_capacity(buf->capacity);
        } while (buf->capacity - buf->size < size);
        buf->mem = checked_realloc(buf->mem, buf->capacity);
    }
    memcpy(&buf->mem[buf->size], data, size);
    buf->size += size;
}
