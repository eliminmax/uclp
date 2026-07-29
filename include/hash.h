/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_HASH_H
#define UCLP_HASH_H
#include <stdint.h>

#include "safety.h"
#include "sized_string.h"
// compute the hash value of the string
uint64_t hash_string(const String s);

#ifdef NDEBUG
[[gnu::const]]
#endif
[[gnu::artificial]]
inline size_t hash_index(uint64_t hash, size_t ht_capacity) {
    REQUIRE(ht_capacity > 0);
    REQUIRE((ht_capacity & (ht_capacity - 1)) == 0);
    return hash & (ht_capacity - 1);
}
#endif /* UCLP_HASH_H */
