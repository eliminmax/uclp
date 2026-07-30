/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "include/sized_string.h"
typedef struct string_pool *_Nonnull StringPool;

[[clang::acquire_handle("pool")]]
StringPool new_pool();
String *_Nonnull intern([[clang::use_handle("pool")]] StringPool, String);
void free_pool([[clang::release_handle("pool")]] StringPool);
