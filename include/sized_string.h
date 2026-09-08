/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef SIZED_STRING_H
#define SIZED_STRING_H
#include <stddef.h>
#include "mem/dynarray.h"

typedef struct {
    size_t len;
    char *text [[gnu::nonstring, clang::counted_by(len)]];
} String;


inline String as_string(const CharBuffer b) {
    return (String){.len = b.size, .text = (char *)b.mem};
}

#endif /* SIZED_STRING_H */
