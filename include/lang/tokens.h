/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
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

TOKEN(L_SQUARE) // "["
TOKEN(R_SQUARE) // "]"
TOKEN(L_CURLY) // "{"
TOKEN(R_CURLY) // "}"
TOKEN(L_PAREN) // "("
TOKEN(R_PAREN) // ")"

// arithmetic tokens

TOKEN(PLUS) // "+"
TOKEN(MINUS) // "-"
TOKEN(MUL) // "*"
TOKEN(DIV) // "/"
TOKEN(MOD) // "%"
TOKEN(SIGNED_MUL) // "$*"
TOKEN(SIGNED_DIV) // "$/"
TOKEN(SIGNED_MOD) // "$%"
TOKEN(PLUS_ASSIGN) // "+="
TOKEN(MINUS_ASSIGN) // "-="
TOKEN(MUL_ASSIGN) // "*="
TOKEN(DIV_ASSIGN) // "/="
TOKEN(MOD_ASSIGN) // "%="
TOKEN(SIGNED_MUL_ASSIGN) // "$*="
TOKEN(SIGNED_DIV_ASSIGN) // "$/="
TOKEN(SIGNED_MOD_ASSIGN) // "$%="


// comparison tokens

TOKEN(EQ) // "=="
TOKEN(NE) // "!="
TOKEN(LT) // "<"
TOKEN(GT) // ">"
TOKEN(LE) // "<="
TOKEN(GE) // ">="
TOKEN(SIGNED_LT) // "$<"
TOKEN(SIGNED_GT) // "$>"
TOKEN(SIGNED_LE) // "$<="
TOKEN(SIGNED_GE) // "$>="

// pointer operations

TOKEN(DEREF) // ".*"
TOKEN(REF) // ".&"

TOKEN(LOGICAL_NOT) // "!"
TOKEN(LOGICAL_AND) // "&&"
TOKEN(LOGICAL_OR) // "||"

// bitwise operations

TOKEN(BIT_NOT) // "~",

TOKEN(BIT_AND) // "&"
TOKEN(BIT_OR) // "|"
TOKEN(BIT_XOR) // "^"

TOKEN(BIT_AND_ASSIGN) // "&="
TOKEN(BIT_OR_ASSIGN) // "|="
TOKEN(BIT_XOR_ASSIGN) // "^="

// bit shifts

TOKEN(SHR_LOG) // ">>"
TOKEN(SHR_ARITH) // "$>>"
TOKEN(SHL) // "<<"

TOKEN(SHR_LOG_ASSIGN) // ">>="
TOKEN(SHR_ARITH_ASSIGN) // "$>>="
TOKEN(SHL_ASSIGN) // "<<="


// misc. tokens

TOKEN(POUND_SIGN) // "#"
TOKEN(ARROW) // "->"
TOKEN(SEMICOLON) // ";"
TOKEN(COLON) // ":"
TOKEN(ASSIGN) // "="
TOKEN(COMMA) // ","

// casts

TOKEN(UNSIGNED_CAST) // ":["
TOKEN(SIGNED_CAST) // "$:["

// keyword tokens

TOKEN(LET) // "let"
TOKEN(STATIC) // "static"
TOKEN(FUNC) // "func"
TOKEN(IF) // "if"
TOKEN(ELIF) // "elif"
TOKEN(ELSE) // "else"
TOKEN(WHILE) // "while"
TOKEN(FOREACH) // "foreach"
TOKEN(IN) // "in"
TOKEN(DYNAMIC) // "dynamic"
TOKEN(OPAQUE) // "opaque"
TOKEN(I8) // "i8"
TOKEN(I16) // "i16"
TOKEN(I32) // "i32"
TOKEN(I64) // "i64"

// literal tokens
TOKEN(STR)
TOKEN(CHAR)
TOKEN(INT_DEC)
TOKEN(INT_BIN)
TOKEN(INT_OCT)
TOKEN(INT_HEX)

TOKEN(IDENT)  // identifier

TOKEN(UNPARSEABLE) // unparseable character/s
TOKEN(EOF)
#undef TOKEN
#ifdef DEFINE_TOKENS
#undef DEFINE_TOKENS
#elifdef DEBUG_TOKENS
#undef DEBUG_TOKENS
}
#endif
