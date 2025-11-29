/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_ALLOC_H
#define UCLP_ALLOC_H

#include <stdio.h>
#include <stdlib.h>

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

[[gnu::returns_nonnull]] inline void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("memory allocation failed in checked_malloc");
        abort();
    }
    return ptr;
}

#endif /* UCLP_ALLOC_H */
