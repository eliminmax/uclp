/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stddef.h>

#include "alloc.h" // IWYU pragma: keep
#include "token.h"

int main() {
    __builtin_trap();
}

// the "home" of the normally-inlined small utility functions defined in
// various project headers

extern inline void destroy_token(Token);
extern inline void destroy_token_sequence(struct token_sequence);

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
