/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

// This file duplicates the logic of the HashTable implementation in quite a few
// ways, but it's simpler, because interned strings won't need to be freed as
// long as the pool is live, and the keys are their own values, so some
// implementation decisions I made for the general HashTable don't carry over.

#include <stdckdint.h>
#include <stdint.h>
#include <string.h>

#include "hash.h"
#include "mem/arena.h"
#include "mem/checked.h"
#include "string_manager.h"

static bool ident_match(String a, String b) {
    return a.len == b.len && memcmp(a.text, b.text, a.len) == 0;
}

typedef struct Interned {
    String *str;
    uint64_t hash;
} Interned;

struct string_pool {
    AllocGroup allocs;
    size_t count;
    size_t capacity;
    [[clang::counted_by(capacity)]] struct Interned *interned;
};

constexpr Interned INIT_STATE[8] = {[0 ... 7] = (Interned){NULL, 0}};

static String EMPTY_STRING;

StringPool new_pool() {
    StringPool pool = checked_malloc(sizeof(struct string_pool));
    pool->allocs = group_create();
    pool->count = 0;
    pool->capacity = 8;
    pool->interned = memcpy(
        checked_malloc(sizeof(INIT_STATE)), &INIT_STATE, sizeof(INIT_STATE)
    );
    uint64_t hash_null = hash_string((String){0, NULL});
    pool->interned[hash_null & 7] = (Interned){.hash = hash_null};
    return pool;
}

static void grow_pool(StringPool pool) {
    size_t new_cap;
    if (ckd_mul(&new_cap, pool->capacity, 2)) [[clang::unlikely]] {
        fprintf(stderr, "FATAL: string pool size overflow.\n");
        abort();
    }

    Interned *new_interned = checked_malloc(new_cap);

    for (size_t i = 0; i < pool->capacity * 2; i += 8) {
        memcpy(&new_interned[i], &INIT_STATE, sizeof(INIT_STATE));
    }

    for (Interned *ptr = pool->interned; ptr < &pool->interned[pool->capacity];
         ptr++) {
        if (ptr->str != NULL) {
            size_t i = hash_index(ptr->hash, new_cap);
            while (new_interned[i].str != NULL) i = hash_index(i + 1, new_cap);
            new_interned[i] = *ptr;
        }
    }
    free(pool->interned);
    pool->interned = new_interned;
}

String *intern(StringPool pool, const String s) {
    if (s.len == 0) return &EMPTY_STRING;

    uint64_t hash = hash_string(s);
    size_t i = hash_index(hash, pool->capacity);

    while (pool->interned[i].hash != hash ||
           !ident_match(*pool->interned[i].str, s)) {

        if (pool->interned[i].str == NULL) {
            pool->count++;

            if (pool->count > pool->capacity * 0.75) {
                grow_pool(pool);
                i = hash_index(hash, pool->capacity);
                while (pool->interned[i].str) {
                    i = hash_index(i + 1, pool->capacity);
                }
            }

            String *dest =
                group_alloc(pool->allocs, sizeof(String), alignof(String));
            dest->len = s.len;
            dest->text = group_alloc(pool->allocs, s.len, 1);
            memcpy(dest->text, s.text, s.len);

            pool->interned[i].hash = hash;
            pool->interned[i].str = dest;
            return dest;
        }
        i = hash_index(i + 1, pool->capacity);
    }

    return pool->interned[i].str;
}

void free_pool(StringPool pool) {
    group_free(pool->allocs);
    free(pool->interned);
    free(pool);
}
