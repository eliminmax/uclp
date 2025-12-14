/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_ALLOC_H
#define UCLP_ALLOC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// A wrapper around `calloc(3)` that calls `perror` and `abort` if the
// underlying call to `calloc` returns `NULL`.
[[gnu::returns_nonnull]] inline void *checked_calloc(
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
[[gnu::returns_nonnull]] inline void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("memory allocation failed in checked_malloc");
        abort();
    }
    return ptr;
}

// A wrapper around `realloc(3)` that calls `perror` and `abort` if the
// underlying call to `realloc` returns `NULL`.
[[gnu::returns_nonnull]] inline void *checked_realloc(
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
[[gnu::returns_nonnull]] inline void *checked_reallocarray(
    void *_Nullable ptr, size_t nmemb, size_t size
) {
    void *new_ptr = reallocarray(ptr, nmemb, size);
    if (!new_ptr) {
        perror("memory allocation failed in checked_reallocarray");
        abort();
    }
    return new_ptr;
}

[[gnu::returns_nonnull]] inline void *checked_aligned_alloc(
    size_t alignment, size_t size
) {
    void *ptr = aligned_alloc(alignment, size);
    if (!ptr) {
        perror("memory allocation failed in checked_aligned_alloc");
        abort();
    }
    return ptr;
}

// I decided to implement handling for grouped allocations with shared
// lifetimes.
//
// Due to some implementation details, I don't know if it's technically an arena
// allocator or not, but it's certainly based on the idea.
typedef struct alloc_group *_Nonnull AllocGroup;

[[clang::acquire_handle("alloc_group")]]
AllocGroup group_create();

void group_free(AllocGroup ag [[clang::release_handle("alloc_group")]]);

// claim the first region within the group of at least `size` that is aligned to
// `1 << align_by` bytes. (`(1 << align_by) <=
void *_Nonnull group_alloc(
    AllocGroup ag [[clang::use_handle("alloc_group")]],
    size_t size,
    size_t alignment
);

#endif /* UCLP_ALLOC_H */
