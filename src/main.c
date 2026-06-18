/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stddef.h>

#include "token.h"

int main() {
    __builtin_trap();
}

// the "home" of the normally-inlined small utility functions defined in
// various project headers

extern inline void destroy_token(Token);
extern inline void destroy_token_sequence(struct token_sequence);
