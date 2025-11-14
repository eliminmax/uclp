/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_ALLOC_H
#define UCLP_ALLOC_H

#include <stdio.h>
#include <stdlib.h>
#ifdef ALLOC_HOME
#define ALLOC_DECL(decl, ...) \
    [[gnu::returns_nonnull]] extern inline decl(__VA_ARGS__); \
    [[gnu::returns_nonnull]] inline decl(__VA_ARGS__)
#else
#define ALLOC_DECL(decl, ...) [[gnu::returns_nonnull]] inline decl(__VA_ARGS__)
#endif /* ALLOC_HOME */

[[gnu::returns_nonnull]]
ALLOC_DECL(void *checked_calloc, size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr) {
        perror("memory allocation failed in checked_calloc");
        abort();
    }
    return ptr;
}


[[gnu::returns_nonnull]]
ALLOC_DECL(void *checked_malloc, size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("memory allocation failed in checked_malloc");
        abort();
    }
    return ptr;
}

#ifdef ALLOC_HOME
#undef ALLOC_HOME
#endif

#endif /* UCLP_ALLOC_H */
