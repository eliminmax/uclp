/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "hash.h"
#include "sized_string.h"

#define STRING_DUP(s) \
    ((String){.len = s.len, \
              .text = memcpy(checked_malloc(s.len), s.text, s.len)})
#define STRING_EQ(a, b) \
    ((a.len == b.len) ? (memcmp(a.text, b.text, a.len) == 0) : false)

// Hash strings with the FNV-1a hash algorithm described at the following page:
//
// https://www.ietf.org/archive/id/draft-eastlake-fnv-35.html
static uint64_t hash_string(const String s) {
    // constants are taken from the algorithm description mentioned above.
    uint64_t hash = UINT64_C(0xcbf29ce4'84222325);
    for (size_t i = 0; i < s.len; ++i) {
        hash ^= (uint8_t)s.text[i];
        hash *= UINT64_C(0x00000100'000001b3);
    }
    return hash;
}

struct ht_entry {
    String id;
    // the data pointer for this entry
    void *_Nonnull data;
    bool occupied;
};

struct hash_table {
    size_t len;
    size_t cap;
    [[clang::counted_by(cap)]] struct ht_entry *_Nonnull entries;
};

HashTable ht_create() {
    struct hash_table *table = checked_malloc(sizeof(struct hash_table));
    table->len = 0;
    table->cap = 8;
    table->entries = checked_calloc(sizeof(struct ht_entry), 8);
    for (int i = 0; i < 8; ++i) table->entries[i].occupied = false;

    return table;
}

void ht_clear(struct hash_table *table, HTEntryCleanupFn callback) {
    for (size_t i = 0; i < table->cap; ++i) {
        if (table->entries[i].occupied) {
            if (callback) callback(table->entries[i].data);
            free(table->entries[i].id.text);
            table->entries[i].occupied = false;
        }
    }
    table->len = 0;
}

void ht_cleanup(struct hash_table *table, HTEntryCleanupFn callback) {
    ht_clear(table, callback);
    free(table->entries);
    free(table);
}

void *_Nullable ht_lookup(
    struct hash_table *const _Nonnull table, const String id
) {
    size_t index = hash_string(id) % table->cap;
    while (table->entries[index].occupied) {
        if (STRING_EQ(id, table->entries[index].id)) {
            return table->entries[index].data;
        }
        index++;
        index %= table->cap;
    }

    return NULL;
}

static void ht_extend_cap(struct hash_table *_Nonnull table) {
    struct ht_entry *new_alloc = checked_calloc(table->cap, 2);
    struct ht_entry *old_alloc = table->entries;

    size_t new_cap = table->cap * 2;
    for (size_t i = 0; i < table->cap; ++i) {
        if (old_alloc[i].occupied) {
            size_t index = hash_string(old_alloc[i].id) % new_cap;
            while (new_alloc[index].occupied) {
                index++;
                index %= new_cap;
            }
            memcpy(&new_alloc[index], &old_alloc[i], sizeof(struct ht_entry));
        }
    }
    table->cap = new_cap;
    table->entries = new_alloc;
    free(old_alloc);
}

void *_Nullable ht_set(
    struct hash_table *_Nonnull table, const String id, void *_Nonnull data
) {
    size_t index = hash_string(id) % table->cap;

    while (table->entries[index].occupied) {
        if (STRING_EQ(id, table->entries[index].id)) {
            void *olddata = table->entries[index].data;
            table->entries[index].data = data;
            return olddata;
        }
        index++;
        index %= table->cap;
    }

    ++table->len;
    if (table->len > table->cap / 2) {
        ht_extend_cap(table);
        index = hash_string(id) % table->cap;
        while (table->entries[index].occupied) {
            // already know that it's not already in the table, so no need to
            // check.
            index++;
            index %= table->cap;
        }
    }
    table->entries[index].occupied = true;
    table->entries[index].data = data;
    table->entries[index].id = STRING_DUP(id);
    return NULL;
}
