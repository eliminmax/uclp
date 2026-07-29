/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stddef.h>
#include <stdint.h>

#include "hash.h"
#include "sized_string.h"

// Hash strings with the FNV-1a hash algorithm described at the following page:
//
// https://www.ietf.org/archive/id/draft-eastlake-fnv-35.html
uint64_t hash_string(const String s) {
    // constants are taken from the algorithm description mentioned above.
    uint64_t hash = UINT64_C(0xcbf29ce4'84222325);
    for (size_t i = 0; i < s.len; ++i) {
        hash ^= (uint8_t)s.text[i];
        hash *= UINT64_C(0x00000100'000001b3);
    }
    return hash;
}

extern inline size_t hash_index(uint64_t hash, size_t ht_capacity);
