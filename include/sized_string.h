/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef SIZED_STRING_H
#define SIZED_STRING_H
#include <stddef.h>

typedef struct {
    char *_Nonnull text;
    size_t len;
} String;

#endif /* SIZED_STRING_H */
