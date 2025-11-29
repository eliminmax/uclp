/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef TOKEN_H
#define TOKEN_H
#include <stdint.h>
#include <stdlib.h>

#include "sized_string.h"

enum token_type : uint8_t {

#define DEFINE_TOKENS
#include "lang/tokens.h"
#undef DEFINE_TOKENS
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

struct token_sequence {
    size_t len;
    Token *_Nonnull tokens [[clang::counted_by(len)]];
};

inline void destroy_token_sequence(struct token_sequence seq) {
    for (size_t i = 0; i < seq.len; ++i) destroy_token(seq.tokens[i]);
    free(seq.tokens);
}

struct token_sequence tokenize(size_t len, const char source[_Nonnull len]);

#endif /* TOKEN_H */
