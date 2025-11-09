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

    // pointer operations
    // ".*"
    DEREF,
    // ".&"
    REF,

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
    // "#"
    POUND_SIGN,
    // "->"
    ARROW,
    // ";"
    SEMICOLON,
    // ":"
    COLON,
    // standalone "="
    ASSIGN,
    // ","
    COMMA,

    // "&&"
    LOG_AND,
    // "||"
    LOG_OR,

    // casts
    // ":["
    UNSIGNED_CAST,
    // "@:["
    SIGNED_CAST,

    // keyword tokens
    // "let"
    KW_LET,
    // "func"
    KW_FUNC,
    // "if"
    KW_IF,
    // "elif"
    KW_ELIF,
    // "else"
    KW_ELSE,
    // "while"
    KW_WHILE,
    // "dynamic"
    KW_DYNAMIC,
    // "opaque"
    KW_OPAQUE,
    // "i8"
    KW_I8,
    // "i16"
    KW_I16,
    // "i32"
    KW_I32,
    // "i64"
    KW_I64,

    // literal
    LITERAL_INT8,
    LITERAL_INT16,
    LITERAL_INT32,
    LITERAL_INT64,
    LITERAL_IDENT,
    LITERAL_STR,
    RAW_INT,

    BLOCK_COMMENT,
    COMMENT,

    // Lex-time Errors
    // Integer literal too large for size
    INT_TOO_LARGE,
    // Integer literal with an unsupported size
    BAD_BIT_LEN,
    // Generic unparsable token
    UNPARSEABLE,

    SEQ_END,
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
