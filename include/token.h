#ifndef TOKEN_H
#define TOKEN_H
#include <stdint.h>
#include <stdlib.h>

#include "sized_string.h"
#include "ucl_nums.h"

enum token_type : uint8_t {
    // bracket-like tokens
    // "[", except when in casts
    L_SQUARE,
    // "]", except when in casts
    R_SQUARE,
    // "{"
    L_CURLY,
    // "}"
    R_CURLY,
    // "("
    L_PAREN,
    // ")"
    R_PAREN,

    // arithmetic tokens
    // "+"
    ARITH_PLUS,
    // standalone "-"
    ARITH_MINUS,
    // standalone "*"
    ARITH_MUL,
    // standalone "/"
    ARITH_DIV,
    // standalone "%"
    ARITH_MOD,
    // "@*"
    ARITH_SIGNED_MUL,
    // "@/"
    ARITH_SIGNED_DIV,
    // "@%"
    ARITH_SIGNED_MOD,

    // comparison tokens
    // "=="
    CMP_EQ,
    // "!="
    CMP_NE,
    // standalone "<"
    CMP_LT,
    // standalone ">"
    CMP_GT,
    // "<="
    CMP_LE,
    // ">="
    CMP_GE,
    // "@<"
    CMP_SIGNED_LT,
    // "@>"
    CMP_SIGNED_GT,
    // "@<="
    CMP_SIGNED_LE,
    // "@>="
    CMP_SIGNED_GE,

    // "&&"
    LOGICAL_AND,
    // "||"
    LOGICAL_OR,

    // bitwise operations
    // standalone "&"
    BIT_AND,
    // "~",
    BIT_NOT,
    // standalone "|"
    BIT_OR,
    // "^"
    BIT_XOR,

    // bit shifts
    // ">>"
    SHR_LOG,
    // "@>>"
    SHR_ARITH,
    // "<<"
    SHL,

    // misc. tokens
    // "->"
    ARROW,
    // ";"
    SEMICOLON,
    // ":"
    COLON,
    // standalone "="
    EQ_ASSIGN,
    // ","
    COMMA,

    // "&&"
    LOG_AND,
    // "||"
    LOG_OR,

    // casts
    // ":[8]"
    CAST_I8,
    // ":[16]"
    CAST_I16,
    // ":[32]"
    CAST_i32,
    // ":[64]"
    CAST_I64,
    // "@:[8]"
    CAST_SIGNED_I8,
    // "@:[16]"
    CAST_SIGNED_I16,
    // "@:[32]"
    CAST_SIGNED_i32,
    // "@:[64]"
    CAST_SIGNED_I64,

    // keyword tokens
    KW_LET,
    KW_FUNC,
    KW_IF,
    KW_ELIF,
    KW_ELSE,
    KW_WHILE,
    KW_I8,
    KW_I16,
    KW_I32,
    KW_I64,

    // literal
    LITERAL_INT8,
    LITERAL_INT16,
    LITERAL_INT32,
    LITERAL_INT64,
    LITERAL_IDENT,
    LITERAL_STR,

    BLOCK_COMMENT,
    COMMENT,

    END,
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
    Token *_Nonnull tokens;
    size_t ntokens;
};

struct token_sequence tokenize(size_t len, const char source[_Nonnull len]);

#endif /* TOKEN_H */
