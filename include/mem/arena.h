/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_ARENA_H
#define UCLP_ARENA_H

#include <stddef.h>

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
// `1 << align_by` bytes.
void *_Nonnull group_alloc(
    AllocGroup ag [[clang::use_handle("alloc_group")]],
    size_t size,
    size_t alignment
);

#endif /* UCLP_ARENA_H */
