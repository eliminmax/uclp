/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef UCLP_STRING_MANAGER
#define UCLP_STRING_MANAGER

#include <stdint.h>

#include "sized_string.h"
#include "mem/arena.h"
typedef struct string_pool *_Nonnull StringPool;

[[clang::acquire_handle("pool")]]
StringPool new_pool(AllocGroup);
String *_Nonnull intern([[clang::use_handle("pool")]] StringPool, String);
void free_pool([[clang::release_handle("pool")]] StringPool);

inline bool pooled_streq(const String a, const String b) {
    return a.len == b.len && a.text == b.text;
}

#endif
