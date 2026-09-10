/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef TOKEN_H
#define TOKEN_H
#include <stdint.h>
#include <stdlib.h>

#include "mem/arena.h"
#include "sized_string.h"

#define DEFINE_TOKENS

typedef enum token_type : uint8_t {

#include "lang/tokens.h"

} TokenType;

// A token of source; `lexeme.text` points into the source code itself.
typedef struct token {
    String lexeme;
    size_t line;
    TokenType type;
} Token;

struct token_sequence tokenize(
    size_t len, AllocGroup ag, const char source[_Nonnull len]
);

void debug_print_token(Token);
#endif /* TOKEN_H */
