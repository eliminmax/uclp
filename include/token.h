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
#include "ucl_nums.h"

enum token_type : uint8_t {
#define DEFINE_TOKENS
    #include "lang/tokens.h"
#undef DEFINE_TOKENS
};

typedef struct token {
    String lexeme;
    size_t line;
    union {
        ucl_i64 literal_i64;
        ucl_i32 literal_i32;
        ucl_i16 literal_i16;
        ucl_i8 literal_i8;
    };
    enum token_type type;
    // use would-be padding to track responsibility for freeing the lexeme
    bool _should_free;
} Token;

inline void destroy_token(Token t) {
    if (t._should_free) free(t.lexeme.text);
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
