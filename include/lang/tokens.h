/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>
#ifdef DEFINE_TOKENS
#define TOKEN(T, fmt, ...) T,
#elifdef DEBUG_TOKENS
#define TOKEN(T, fmt, ...) \
    case T: \
        printf(fmt __VA_OPT__(, ) __VA_ARGS__); \
        break;

switch (TOK.type) {
#elifndef TOKEN
#warning "define TOKEN, DEFINE_TOKENS or DEBUG_TOKENS before including tokens.h"
#define TOKEN(T, fmt, ...)
#endif
#define SIMPLE_TOKEN(T) TOKEN(T, #T "<line %zu, lexeme %s>\n", TOK.line, lexeme)

// bracket-like tokens
// "[", except when in casts
SIMPLE_TOKEN(L_SQUARE)
// "]", except when in casts
SIMPLE_TOKEN(R_SQUARE)
// "{"
SIMPLE_TOKEN(L_CURLY)
// "}"
SIMPLE_TOKEN(R_CURLY)
// "("
SIMPLE_TOKEN(L_PAREN)
// ")"
SIMPLE_TOKEN(R_PAREN)

// arithmetic tokens
// "+"
SIMPLE_TOKEN(ARITH_PLUS)
// standalone "-"
SIMPLE_TOKEN(ARITH_MINUS)
// standalone "*"
SIMPLE_TOKEN(ARITH_MUL)
// standalone "/"
SIMPLE_TOKEN(ARITH_DIV)
// standalone "%"
SIMPLE_TOKEN(ARITH_MOD)
// "@*"
SIMPLE_TOKEN(ARITH_SIGNED_MUL)
// "@/"
SIMPLE_TOKEN(ARITH_SIGNED_DIV)
// "@%"
SIMPLE_TOKEN(ARITH_SIGNED_MOD)

// comparison tokens
// "=="
SIMPLE_TOKEN(CMP_EQ)
// "!="
SIMPLE_TOKEN(CMP_NE)
// standalone "<"
SIMPLE_TOKEN(CMP_LT)
// standalone ">"
SIMPLE_TOKEN(CMP_GT)
// "<="
SIMPLE_TOKEN(CMP_LE)
// ">="
SIMPLE_TOKEN(CMP_GE)
// "@<"
SIMPLE_TOKEN(CMP_SIGNED_LT)
// "@>"
SIMPLE_TOKEN(CMP_SIGNED_GT)
// "@<="
SIMPLE_TOKEN(CMP_SIGNED_LE)
// "@>="
SIMPLE_TOKEN(CMP_SIGNED_GE)

// pointer operations
// ".*"
SIMPLE_TOKEN(DEREF)
// ".&"
SIMPLE_TOKEN(REF)

// "&&"
SIMPLE_TOKEN(LOGICAL_AND)
// "||"
SIMPLE_TOKEN(LOGICAL_OR)

// bitwise operations
// standalone "&"
SIMPLE_TOKEN(BIT_AND)
// "~",
SIMPLE_TOKEN(BIT_NOT)
// standalone "|"
SIMPLE_TOKEN(BIT_OR)
// "^"
SIMPLE_TOKEN(BIT_XOR)

// bit shifts
// ">>"
SIMPLE_TOKEN(SHR_LOG)
// "@>>"
SIMPLE_TOKEN(SHR_ARITH)
// "<<"
SIMPLE_TOKEN(SHL)

// misc. tokens
// "#"
SIMPLE_TOKEN(POUND_SIGN)
// "->"
SIMPLE_TOKEN(ARROW)
// ";"
SIMPLE_TOKEN(SEMICOLON)
// ":"
SIMPLE_TOKEN(COLON)
// standalone "="
SIMPLE_TOKEN(ASSIGN)
// ","
SIMPLE_TOKEN(COMMA)

// casts
// ":["
SIMPLE_TOKEN(UNSIGNED_CAST)
// "@:["
SIMPLE_TOKEN(SIGNED_CAST)

// keyword tokens
// "let"
SIMPLE_TOKEN(KW_LET)
// "static"
SIMPLE_TOKEN(KW_STATIC)
// "func"
SIMPLE_TOKEN(KW_FUNC)
// "if"
SIMPLE_TOKEN(KW_IF)
// "elif"
SIMPLE_TOKEN(KW_ELIF)
// "else"
SIMPLE_TOKEN(KW_ELSE)
// "while"
SIMPLE_TOKEN(KW_WHILE)
// "dynamic"
SIMPLE_TOKEN(KW_DYNAMIC)
// "opaque"
SIMPLE_TOKEN(KW_OPAQUE)
// "i8"
SIMPLE_TOKEN(KW_I8)
// "i16"
SIMPLE_TOKEN(KW_I16)
// "i32"
SIMPLE_TOKEN(KW_I32)
// "i64"
SIMPLE_TOKEN(KW_I64)

// 8-bit literal
TOKEN(
    LITERAL_INT8,
    "LITERAL_INT8<line %zu, lexeme %s, value %" PRIu8 ">\n",
    TOK.line,
    lexeme,
    TOK.literal_i8.u
)
// 16-bit literal
TOKEN(
    LITERAL_INT16,
    "LITERAL_INT16<line %zu, lexeme %s, value %" PRIu16 ">\n",
    TOK.line,
    lexeme,
    TOK.literal_i16.u
)
// explicitly-sized 32-bit literal
TOKEN(
    LITERAL_INT32,
    "LITERAL_INT32<line %zu, lexeme %s, value %" PRIu32 ">\n",
    TOK.line,
    lexeme,
    TOK.literal_i32.u
)
// 64-bit literal
TOKEN(
    LITERAL_INT64,
    "LITERAL_INT64<line %zu, lexeme %s, value %" PRIu64 ">\n",
    TOK.line,
    lexeme,
    TOK.literal_i64.u
)
SIMPLE_TOKEN(LITERAL_STR)
// implicitly-sized 32-bit literal
TOKEN(
    RAW_INT,
    "RAW_INT<line %zu, lexeme %s, value %" PRIu32 ">\n",
    TOK.line,
    lexeme,
    TOK.literal_i32.u
)

SIMPLE_TOKEN(LITERAL_IDENT)
SIMPLE_TOKEN(BLOCK_COMMENT)
SIMPLE_TOKEN(COMMENT)

// Lex-time Errors
// Integer literal too large for size
SIMPLE_TOKEN(INT_TOO_LARGE)
// Integer literal with an unsupported size
SIMPLE_TOKEN(BAD_BIT_LEN)
// Generic unparsable token
SIMPLE_TOKEN(UNPARSEABLE)

TOKEN(SEQ_END, "SEQ_END\n")
#undef TOKEN
#undef SIMPLE_TOKEN
#ifdef DEFINE_TOKENS
#undef DEFINE_TOKENS
#elifdef DEBUG_TOKENS
#undef DEBUG_TOKENS
}
#endif
