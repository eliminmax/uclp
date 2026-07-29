/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_MEM_CHECKED_H
#define UCLP_MEM_CHECKED_H
#include <stdio.h>
#include <stdlib.h>

// A wrapper around `calloc(3)` that calls `perror` and `abort` if the
// underlying call to `calloc` returns `NULL`.
[[gnu::returns_nonnull]] inline void *_Nonnull checked_calloc(
    size_t nmemb, size_t size
) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        perror("memory allocation failed in checked_calloc");
        abort();
    }
    return ptr;
}

// A wrapper around `malloc(3)` that calls `perror` and `abort` if the
// underlying call to `malloc` returns `NULL`.
[[gnu::returns_nonnull]] inline void *_Nonnull checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("memory allocation failed in checked_malloc");
        abort();
    }
    return ptr;
}

// A wrapper around `realloc(3)` that calls `perror` and `abort` if the
// underlying call to `realloc` returns `NULL`.
[[gnu::returns_nonnull]] inline void *_Nonnull checked_realloc(
    void *_Nullable ptr, size_t size
) {
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        perror("memory allocation failed in checked_realloc");
        abort();
    }
    return new_ptr;
}

// A wrapper around `reallocarray(3)` that calls `perror` and `abort` if the
// underlying call to `reallocarray` returns `NULL`.
[[gnu::returns_nonnull]] inline void *_Nonnull checked_reallocarray(
    void *_Nullable ptr, size_t nmemb, size_t size
) {
    void *new_ptr = reallocarray(ptr, nmemb, size);
    if (!new_ptr) {
        perror("memory allocation failed in checked_reallocarray");
        abort();
    }
    return new_ptr;
}

[[gnu::returns_nonnull]] inline void *_Nonnull checked_aligned_alloc(
    size_t alignment, size_t size
) {
    void *ptr = aligned_alloc(alignment, size);
    if (!ptr) {
        perror("memory allocation failed in checked_aligned_alloc");
        abort();
    }
    return ptr;
}

#endif /* UCLP_MEM_CHECKED_H */
