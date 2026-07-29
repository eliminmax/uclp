/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <string.h>

#include "mem/checked.h"
#include "table.h"

#define STRING_DUP(s) \
    ((String){ \
        .len = s.len, .text = memcpy(checked_malloc(s.len), s.text, s.len) \
    })
#define STRING_EQ(a, b) \
    ((a.len == b.len) ? (memcmp(a.text, b.text, a.len) == 0) : false)

struct ht_entry {
    String id;
    uint64_t hash;
    // the data pointer for this entry
    void *_Nullable data;

    enum : int8_t {
        HT_ENTRY_TOMBSTONE = -1,
        HT_ENTRY_EMPTY = 0,
        HT_ENTRY_OCCUPIED = 1,
    } status;
};

struct hash_table {
    size_t count;
    size_t capacity;
    [[clang::counted_by(capacity)]] struct ht_entry *_Nonnull entries;
};

HashTable ht_create() {
    struct hash_table *table = checked_malloc(sizeof(struct hash_table));
    table->count = 0;
    table->capacity = 8;
    table->entries = checked_calloc(sizeof(struct ht_entry), 8);
    for (int i = 0; i < 8; ++i) table->entries[i].status = HT_ENTRY_EMPTY;

    return table;
}

void ht_clear(struct hash_table *table, HTEntryCleanupFn callback) {
    for (size_t i = 0; i < table->capacity; ++i) {
        if (table->entries[i].status) {
            if (callback) callback(table->entries[i].data);
            free(table->entries[i].id.text);
            table->entries[i].status = false;
        }
    }
    table->count = 0;
}

void ht_cleanup(struct hash_table *table, HTEntryCleanupFn callback) {
    ht_clear(table, callback);
    free(table->entries);
    free(table);
}

void *_Nullable ht_lookup(
    struct hash_table *const _Nonnull table, const String id
) {
    size_t index = hash_index(hash_string(id), table->capacity);
    while (table->entries[index].status) {
        if (STRING_EQ(id, table->entries[index].id)) {
            return table->entries[index].data;
        }
        index++;
        index %= table->capacity;
    }

    return NULL;
}

static void ht_extend_cap(struct hash_table *_Nonnull table) {
    struct ht_entry *new_alloc = checked_calloc(table->capacity, 2);
    struct ht_entry *old_alloc = table->entries;

    size_t new_cap = table->capacity * 2;
    for (size_t i = 0; i < table->capacity; ++i) {
        if (old_alloc[i].status) {
            size_t index = hash_string(old_alloc[i].id) % new_cap;
            while (new_alloc[index].status) {
                index++;
                index %= new_cap;
            }
            memcpy(&new_alloc[index], &old_alloc[i], sizeof(struct ht_entry));
        }
    }
    table->capacity = new_cap;
    table->entries = new_alloc;
    free(old_alloc);
}

void *_Nullable ht_set(
    struct hash_table *_Nonnull table, const String id, void *_Nonnull data
) {
    size_t index = hash_index(hash_string(id), table->capacity);
    struct ht_entry *entry;
    struct ht_entry *tombstone = NULL;
    while (entry = &table->entries[index], entry->status) {
        if (entry->status == HT_ENTRY_TOMBSTONE) {
            if (!tombstone) tombstone = entry;
        } else if (entry->status && STRING_EQ(id, entry->id)) {
            void *olddata = entry->data;
            entry->data = data;
            return olddata;
        }
        index++;
        index %= table->capacity;
    }

    if (tombstone) {
        entry = tombstone;
    } else {
        ++table->count;
        if (table->count > table->capacity * 0.75) {
            ht_extend_cap(table);
            index = hash_index(hash_string(id), table->capacity);
            while (table->entries[index].status == HT_ENTRY_OCCUPIED) {
                // already know that it's not already in the table, so no need
                // to check.
                index++;
                index %= table->capacity;
            }
        }

        entry = &table->entries[index];
    }

    entry->status = HT_ENTRY_OCCUPIED;
    entry->data = data;
    entry->id = STRING_DUP(id);
    return NULL;
}
