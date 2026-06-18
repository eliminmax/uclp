/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#include "mem/checked.h"

[[gnu::returns_nonnull]]
extern inline void *checked_calloc(size_t, size_t);
[[gnu::returns_nonnull]]
extern inline void *checked_malloc(size_t);
[[gnu::returns_nonnull]]
extern inline void *checked_realloc(void *_Nullable, size_t);
[[gnu::returns_nonnull]]
extern inline void *checked_reallocarray(void *_Nullable, size_t, size_t);
[[gnu::returns_nonnull]]
extern inline void *checked_aligned_alloc(size_t, size_t);
