/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 */

#include "mem/arena.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "mem/checked.h"
#include "safety.h"
#include "typenames.h"

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
    struct large_alloc *_Nullable last_large;
};

struct alloc_segment {
    alignas(max_align_t) uchar data [[gnu::nonstring]][ALLOC_SEGMENT_SIZE];
};

// a singly-linked list of large allocations
struct large_alloc {
    void *_Nonnull ptr;
    struct large_alloc *_Nullable next;
};

// Might as well ensure, as the `struct large_alloc` is stored inline
static_assert(sizeof(struct large_alloc) < MAX_ALLOC_INLINE_SIZE);

// used for allocations <= MAX_ALLOC_INLINE_SIZE
static void *_Nonnull group_alloc_small(
    struct alloc_group *_Nonnull ag [[clang::use_handle("alloc_group")]],
    size_t size,
    size_t alignment
) {
    REQUIRE(size <= MAX_ALLOC_INLINE_SIZE);

    struct segment_handler *handler = ag->segments;
    while (size > ALLOC_SEGMENT_SIZE - handler->used) {
        if (!handler->next) {
            handler->next = checked_malloc(sizeof(struct segment_handler));
            handler = handler->next;

            handler->segment = checked_aligned_alloc(
                alignof(struct segment_handler), ALLOC_SEGMENT_SIZE
            );
            handler->used = size;
            handler->next = NULL;
            return handler->segment->data;
        }
        handler = handler->next;
    }
    size_t start = handler->used;
    if (start % alignment != 0) start += alignment - (start % alignment);
    handler->used = start + size;
    return &(handler->segment->data[start]);
}

void group_claim(
    AllocGroup ag [[clang::use_handle("alloc_group")]], void *_Nonnull ptr
) {
    struct large_alloc *a = group_alloc_small(
        ag, sizeof(struct large_alloc), alignof(struct large_alloc)
    );
    a->ptr = ptr;
    a->next = NULL;

    if (ag->large_allocs) {
        ag->last_large->next = a;
        ag->last_large = a;
    } else {
        ag->last_large = ag->large_allocs = a;
    }
}

[[clang::acquire_handle("alloc_group")]] AllocGroup group_create() {
    AllocGroup arena = checked_malloc(sizeof(struct alloc_group));

    struct segment_handler *head_seg =
        checked_malloc(sizeof(struct segment_handler));
    head_seg->used = 0;
    head_seg->next = NULL;
    head_seg->segment = checked_malloc(ALLOC_SEGMENT_SIZE);

    arena->segments = head_seg;
    arena->large_allocs = NULL;
    arena->last_large = NULL;

    return arena;
}

void *_Nonnull group_alloc(
    struct alloc_group *_Nonnull ag [[clang::use_handle("alloc_group")]],
    size_t size,
    size_t alignment
) {
    assert(alignment <= alignof(max_align_t));

    if (size > MAX_ALLOC_INLINE_SIZE) {
        void *ptr = checked_aligned_alloc(alignment, size);
        group_claim(ag, ptr);
        return ptr;
    } else {
        return group_alloc_small(ag, size, alignment);
    }
}

void group_free(
    struct alloc_group *_Nonnull ag [[clang::release_handle("alloc_group")]]
) {
    // metadata of large allocations are stored within the arena, so make sure
    // to handle them first to avoid use-after-free
    struct large_alloc *large = ag->large_allocs;
    while (large) {
        free(large->ptr);
        large = large->next;
    }

    struct segment_handler *segment, *next;
    segment = ag->segments;
    do {
        next = segment->next;
        free(segment->segment);
        free(segment);
        segment = next;
    } while (next);
    free(ag);
}
