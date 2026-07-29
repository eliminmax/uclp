/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 */

#include "mem/arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "mem/checked.h"
static constexpr size_t ALLOC_SEGMENT_SIZE = sizeof(intmax_t[0x1000]);
// the maximum size to keep within a segment
static constexpr size_t MAX_ALLOC_INLINE_SIZE = ALLOC_SEGMENT_SIZE / 2;

// a singly-linked list of segment handlers
struct segment_handler {
    size_t used;
    struct segment_handler *_Nullable next;
    struct alloc_segment *_Nonnull segment;
};

struct alloc_group {
    struct segment_handler *_Nonnull segments;
    struct large_alloc *_Nullable large_allocs;
};

struct alloc_segment {
    alignas(max_align_t) char data[ALLOC_SEGMENT_SIZE];
};

// a singly-linked list of large allocations
struct large_alloc {
    void *_Nonnull ptr;
    struct large_alloc *_Nullable next;
};

[[clang::acquire_handle("alloc_group")]] AllocGroup group_create() {
    AllocGroup arena = checked_malloc(sizeof(struct alloc_group));
    arena->segments = checked_malloc(sizeof(struct alloc_segment));

    struct segment_handler *head_seg =
        checked_malloc(sizeof(struct segment_handler));

    arena->segments = head_seg;
    arena->large_allocs = NULL;

    return arena;
}

void *_Nonnull group_alloc(
    struct alloc_group *_Nonnull arena [[clang::use_handle("alloc_group")]],
    size_t size,
    size_t alignment
) {
    assert(alignment <= alignof(max_align_t));

    if (size > MAX_ALLOC_INLINE_SIZE) {
        struct large_alloc **next_alloc = &arena->large_allocs;

        // find the tail of the next allocation
        while (*next_alloc) next_alloc = &(*next_alloc)->next;

        // store the linked list node itself within `arena`
        *next_alloc = group_alloc(
            arena, sizeof(struct large_alloc), alignof(struct large_alloc)
        );

        (*next_alloc)->next = NULL;
        (*next_alloc)->ptr = checked_aligned_alloc(alignment, size);
        return (*next_alloc)->ptr;
    }

    struct segment_handler *handler = arena->segments;
    assert(handler != NULL);

    while (size > ALLOC_SEGMENT_SIZE - handler->used) {
        if (!handler->next) {
            handler->next = checked_malloc(sizeof(struct segment_handler));
            handler = handler->next;

            handler->segment =
                checked_aligned_alloc(alignment, ALLOC_SEGMENT_SIZE);
            handler->used = size;
            handler->next = NULL;
            return handler->segment->data;
        }
        handler = handler->next;
    }
    size_t start = handler->used;
    if (start % alignment != 0) start += alignment - (start % alignment);
    assert(start + size < ALLOC_SEGMENT_SIZE);
    handler->used = start + size;
    return &(handler->segment->data[start]);
}

void group_free(
    struct alloc_group *_Nonnull arena [[clang::release_handle("alloc_group")]]
) {
    // metadata of large allocations are stored within the arena, so make sure
    // to handle them first to avoid use-after-free
    struct large_alloc *large = arena->large_allocs;
    while (large) {
        free(large->ptr);
        large = large->next;
    }

    struct segment_handler *segment, *next = arena->segments;
    while ((segment = next)) {
        next = segment->next;
        free(segment->segment);
        free(segment);
    }
    free(arena);
}
