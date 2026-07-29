/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
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

enum token_type : uint8_t {

#include "lang/tokens.h"

};

typedef struct token {
    String lexeme;
    size_t line;

    enum token_type type;
    bool should_free_text;
} Token;

inline void destroy_token(Token t) {
    if (t.should_free_text) free(t.lexeme.text);
}

struct token_sequence tokenize(
    size_t len, AllocGroup ag, const char source[_Nonnull len]
);

#endif /* TOKEN_H */
