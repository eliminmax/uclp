/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_HASH_H
#define UCLP_HASH_H
#include <stdint.h>

#include "sized_string.h"
// An opaque pointer to a HashTable that maps `String` -> `void *_Nonnull`
typedef struct hash_table *_Nonnull HashTable;

// A cleanup callback for use when removing entries with ht_clear, or
// deallocating a table with ht_cleanup
typedef void (*_Nullable HTEntryCleanupFn)(void *_Nonnull);

// create a new HashTable
HashTable ht_create();

// compute the hash value of the string
uint64_t hash_string(const String s);

// clean up a HashTable, freeing its internal allocations then freeing it
// itself. Second argument is a callback function to clean up the entries - e.g.
// if they need to be `free`d, `free` would be an appropriate choice.
void ht_cleanup(HashTable, HTEntryCleanupFn);

// clear all entries within the table, leaving the table intact for future use
void ht_clear(HashTable, HTEntryCleanupFn);

// look up the provided String in the HashTable. Return the matched value if
// found, otherwise returns NULL
void *_Nullable ht_lookup(const HashTable, const String);

// set the entry for `id` to `data`. If a previous piece of data was found, then
// it is returned for the caller to clean up. Otherwise, NULL is returned.
void *_Nullable ht_set(HashTable, const String, void *_Nonnull);

#endif /* UCLP_HASH_H */
