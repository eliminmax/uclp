/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifdef DEFINE_TOKENS
#define TOKEN(T) TOKEN_##T,
#elifdef DEBUG_TOKENS
#define TOKEN(T) \
    case TOKEN_##T: \
        printf("<" #T ", line %zu, lexeme `%s`>\n", TOK.line, lexeme); \
        break;

switch (TOK.type) {
#elifndef TOKEN
#warning "define TOKEN, DEFINE_TOKENS or DEBUG_TOKENS before including tokens.h"
#define TOKEN(T)
#endif

// bracket-like tokens

// "[", except when in casts
TOKEN(L_SQUARE)
// "]", except when in casts
TOKEN(R_SQUARE)
// "{"
TOKEN(L_CURLY)
// "}"
TOKEN(R_CURLY)
// "("
TOKEN(L_PAREN)
// ")"
TOKEN(R_PAREN)

// arithmetic tokens

// "+"
TOKEN(ARITH_PLUS)
// standalone "-"
TOKEN(ARITH_MINUS)
// standalone "*"
TOKEN(ARITH_MUL)
// standalone "/"
TOKEN(ARITH_DIV)
// standalone "%"
TOKEN(ARITH_MOD)
// "@*"
TOKEN(ARITH_SIGNED_MUL)
// "@/"
TOKEN(ARITH_SIGNED_DIV)
// "@%"
TOKEN(ARITH_SIGNED_MOD)

// comparison tokens

// "=="
TOKEN(CMP_EQ)
// "!="
TOKEN(CMP_NE)
// standalone "<"
TOKEN(CMP_LT)
// standalone ">"
TOKEN(CMP_GT)
// "<="
TOKEN(CMP_LE)
// ">="
TOKEN(CMP_GE)
// "@<"
TOKEN(CMP_SIGNED_LT)
// "@>"
TOKEN(CMP_SIGNED_GT)
// "@<="
TOKEN(CMP_SIGNED_LE)
// "@>="
TOKEN(CMP_SIGNED_GE)

// pointer operations

// ".*"
TOKEN(DEREF)
// ".&"
TOKEN(REF)

// "&&"
TOKEN(LOGICAL_AND)
// "||"
TOKEN(LOGICAL_OR)

// bitwise operations

// standalone "&"
TOKEN(BIT_AND)
// "~",
TOKEN(BIT_NOT)
// standalone "|"
TOKEN(BIT_OR)
// "^"
TOKEN(BIT_XOR)

// bit shifts

// ">>"
TOKEN(SHR_LOG)
// "@>>"
TOKEN(SHR_ARITH)
// "<<"
TOKEN(SHL)

// misc. tokens

// "#"
TOKEN(POUND_SIGN)
// "->"
TOKEN(ARROW)
// ";"
TOKEN(SEMICOLON)
// ":"
TOKEN(COLON)
// standalone "="
TOKEN(ASSIGN)
// ","
TOKEN(COMMA)

// casts

// ":["
TOKEN(UNSIGNED_CAST)
// "@:["
TOKEN(SIGNED_CAST)

// keyword tokens

// "let"
TOKEN(KW_LET)
// "static"
TOKEN(KW_STATIC)
// "func"
TOKEN(KW_FUNC)
// "if"
TOKEN(KW_IF)
// "elif"
TOKEN(KW_ELIF)
// "else"
TOKEN(KW_ELSE)
// "while"
TOKEN(KW_WHILE)
// "foreach"
TOKEN(KW_FOREACH)
// "in"
TOKEN(KW_IN)
// "dynamic"
TOKEN(KW_DYNAMIC)
// "opaque"
TOKEN(KW_OPAQUE)
// "i8"
TOKEN(KW_I8)
// "i16"
TOKEN(KW_I16)
// "i32"
TOKEN(KW_I32)
// "i64"
TOKEN(KW_I64)

// literal tokens
TOKEN(LITERAL_STR)
TOKEN(LITERAL_CHAR)
TOKEN(LITERAL_INT_DEC)
TOKEN(LITERAL_INT_BIN)
TOKEN(LITERAL_INT_OCT)
TOKEN(LITERAL_INT_HEX)

TOKEN(IDENT)

// unparsable token
TOKEN(UNPARSEABLE)
#undef TOKEN
#ifdef DEFINE_TOKENS
#undef DEFINE_TOKENS
#elifdef DEBUG_TOKENS
#undef DEBUG_TOKENS
}
#endif
