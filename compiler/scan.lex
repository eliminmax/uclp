/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

%{
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sized_string.h"
#include "token.h"

struct token_sequence_builder {
    struct token_sequence sequence;
    size_t capacity;
};

#define YY_NO_INPUT
#define YY_NO_UNPUT
extern inline void destroy_token(Token);
extern inline void destroy_token_sequence(struct token_sequence);

static void add_token(struct token_sequence_builder *builder, Token *token) {
    assert(builder->sequence.len <= builder->capacity);
    if (builder->sequence.len == builder->capacity) {
        size_t new_capacity = builder->sequence.len * 2;
        if (new_capacity >= SIZE_MAX / sizeof(Token)) {
            fputs("Not enough memory to build token sequence\n", stderr);
            abort();
        }
        Token *new_tokens =
            reallocarray(builder->sequence.tokens, new_capacity, sizeof(Token));
        if (!new_tokens) {
            perror("Failed to exend sequence token buffer");
            abort();
        }
        builder->capacity = new_capacity;
        builder->sequence.tokens = new_tokens;
    }

    memcpy(
        &builder->sequence.tokens[builder->sequence.len],
        token,
        sizeof(Token)
    );
    builder->sequence.len++;
}

[[gnu::returns_nonnull]]
static void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        perror("memory allocation failed");
        abort();
    }
    return ptr;
}

// return a heap-allocated String containing a copy of the current text match
static String current_lexeme(void) {
    String str = (String){
        .len = yyleng, .text = memcpy(checked_malloc(yyleng), yytext, yyleng)
    };
    return str;
}

static void parse_int(Token *token, const char *start, int base) {
    char *end;
    token->_should_free = true;
    token->lexeme = current_lexeme();
    token->line = yylineno;
    unsigned long long val = strtoull(start, &end, base);
    if (*end == '#') {
        errno = 0;
        unsigned long long bit_len = strtoll(&end[1], NULL, 10);
        if (errno) bit_len = 0;
        switch (bit_len) {
            case 8:
                if (val > UINT8_MAX) goto too_large;
                token->type = LITERAL_INT8, token->literal_i8.u = val;
                break;
            case 16:
                if (val > UINT16_MAX) goto too_large;
                token->type = LITERAL_INT16, token->literal_i16.u = val;
                break;
            case 32:
                if (val > UINT32_MAX) goto too_large;
                token->type = LITERAL_INT32, token->literal_i32.u = val;
                break;
            case 64:
                if (val > UINT64_MAX) goto too_large;
                token->type = LITERAL_INT64, token->literal_i64.u = val;
                break;
                break;
            default:
                token->type = BAD_BIT_LEN;
        }
    } else {
        if (val > UINT32_MAX) goto too_large;
        token->type = RAW_INT, token->literal_i32.u = val;
    }
    return;
too_large:
    token->type = INT_TOO_LARGE;
}

static void finalize(struct token_sequence_builder *builder) {
    assert(builder->sequence.len < SIZE_MAX - 1);

    Token *addr = reallocarray(builder->sequence.tokens, builder->sequence.len + 1, sizeof(Token));
    if (!addr) {
        perror("Failed to shrink allocation of token sequence");
        abort();
    }
    builder->sequence.tokens = addr;
    builder->sequence.tokens[builder->sequence.len++] = (Token) {
        .type = SEQ_END,
        .line = yylineno,
    };
}

#define YY_DECL struct token_sequence yylex(void)
#define yyterminate() finalize(&builder); return builder.sequence

%}

%option noyywrap yylineno nodefault
%pointer

whitespace [ \n\t]+

escaped_char \\[ntr'\"\\]|\\x[0-8a-f]{2}|\\[0-3]?([0-7]{1,2})
char_literal '([^'\\\n\r]|{escaped_char})'
string_literal \"([^"\\\n\r]|{escaped_char})*\"

int_size_suffix (#[0-9]+)?
dec_literal [0-9]+{int_size_suffix}
bin_literal 0b[01]+{int_size_suffix}
oct_literal 0o[0-7]+{int_size_suffix}
hex_literal 0x[0-9a-fA-F]+{int_size_suffix}
ident [_a-zA-Z][_0-9a-zA-Z]*

comment [/]{2}.*

%%
    #define BASIC_TOKEN(t, l) \
        token = (Token){ \
            .lexeme = (String){.text = l, .len = sizeof(l) - 1}, \
            .type = t, \
            .line = yylineno, \
        }; \
        add_token(&builder, &token);
    struct token_sequence_builder builder = (struct token_sequence_builder){
        .sequence = (struct token_sequence){
            .len = 0,
            .tokens = checked_malloc(sizeof(Token)),
        },
        .capacity = 1
    };
    Token token;

{whitespace}

"let" { BASIC_TOKEN(KW_LET, "let"); }
"func" { BASIC_TOKEN(KW_FUNC, "func"); }
"if" { BASIC_TOKEN(KW_IF, "if"); }
"elif" { BASIC_TOKEN(KW_ELIF, "elif"); }
"else" { BASIC_TOKEN(KW_ELSE, "else"); }
"while" { BASIC_TOKEN(KW_WHILE, "while"); }
"dynamic" { BASIC_TOKEN(KW_DYNAMIC, "dynamic"); }
"opaque" { BASIC_TOKEN(KW_OPAQUE, "opaque"); }
"i8" { BASIC_TOKEN(KW_I8, "i8"); }
"i16" { BASIC_TOKEN(KW_I16, "i16"); }
"i32" { BASIC_TOKEN(KW_I32, "i32"); }
"i64" { BASIC_TOKEN(KW_I64, "i64"); }

{ident} {
    token = (Token){
        .type = LITERAL_IDENT,
        .lexeme = current_lexeme(), 
        .line = yylineno,
        ._should_free = true,
    };
    add_token(&builder, &token);
}

{char_literal} {
    char val;
    char *s = yytext + 1;
    if (*s == '\\') {
        switch(s[1]) {
            case 'n': val = '\n'; break;
            case 'r': val = '\r'; break;
            case 't': val = '\t'; break;
            case 'x':
                val = strtoull(&s[2], NULL, 16);
                break;
            case '\\':
            case '\"':
            case '\'':
                val = s[1];
                break;
            default:
                assert(isdigit(s[1]));
                val = strtoull(&s[1], NULL, 8);
        }
    } else {
        val = *s;
    }
    token = (Token) {
        .lexeme = current_lexeme(),
        .type = LITERAL_INT8,
        .line = yylineno,
        .literal_i8 = (ucl_i8){.s = val},
        ._should_free = true,
    };
    add_token(&builder, &token);
}

{string_literal} {
    token = (Token){
        .type = LITERAL_STR,
        .lexeme = current_lexeme(), 
        .line = yylineno,
        ._should_free = true,
    };
    add_token(&builder, &token);
}

{dec_literal} { parse_int(&token, yytext, 10); add_token(&builder, &token); }
{hex_literal} { parse_int(&token, yytext + 2, 16); add_token(&builder, &token); }
{oct_literal} { parse_int(&token, yytext + 2, 8); add_token(&builder, &token); }
{bin_literal} { parse_int(&token, yytext + 2, 2); add_token(&builder, &token); }

{comment} {
    token = (Token){
        .type = COMMENT,
        .line = yylineno,
        ._should_free = true,
        .lexeme = current_lexeme(),
    };
    add_token(&builder, &token);
}

"/*"([^/]|[^*][/])*"*/" {
    token = (Token){
        .type = BLOCK_COMMENT,
        .line = yylineno,
        ._should_free = true,
        .lexeme = current_lexeme(),
    };
    add_token(&builder, &token);
}

"[" { BASIC_TOKEN(L_SQUARE, "["); }
"]" { BASIC_TOKEN(R_SQUARE, "]"); }
"{" { BASIC_TOKEN(L_CURLY, "{"); }
"}" { BASIC_TOKEN(R_CURLY, "}"); }
"(" { BASIC_TOKEN(L_PAREN, "("); }
")" { BASIC_TOKEN(R_PAREN, ")"); }

"+" { BASIC_TOKEN(ARITH_PLUS, "+"); }
"-"  { BASIC_TOKEN(ARITH_MINUS, "-"); }
"*" { BASIC_TOKEN(ARITH_MUL, "*"); }
"/" { BASIC_TOKEN(ARITH_DIV, "/"); }
"%" { BASIC_TOKEN(ARITH_MOD, "%"); }
"@*" { BASIC_TOKEN(ARITH_SIGNED_MUL, "@*"); }
"@/" { BASIC_TOKEN(ARITH_SIGNED_DIV, "@/"); }
"@%" { BASIC_TOKEN(ARITH_SIGNED_MOD, "@%"); }

"==" { BASIC_TOKEN(CMP_EQ, "=="); }
"!=" { BASIC_TOKEN(CMP_NE, "!="); }
">" { BASIC_TOKEN(CMP_GT, ">"); }
"<" { BASIC_TOKEN(CMP_LT, "<"); }
">=" { BASIC_TOKEN(CMP_GE, ">="); }
"<=" { BASIC_TOKEN(CMP_LE, "<="); }
"@>" { BASIC_TOKEN(CMP_SIGNED_GT, "@>"); }
"@<" { BASIC_TOKEN(CMP_SIGNED_LT, "@<"); }
"@>=" { BASIC_TOKEN(CMP_SIGNED_GE, "@>="); }
"@<=" { BASIC_TOKEN(CMP_SIGNED_LE, "@<="); }

".*" { BASIC_TOKEN(DEREF, ".*"); }
".&" { BASIC_TOKEN(REF, ".&"); }

"&&" { BASIC_TOKEN(LOG_AND, "&&"); }
"||" { BASIC_TOKEN(LOG_OR, "||"); }

"&" { BASIC_TOKEN(BIT_AND, "&"); }
"~" { BASIC_TOKEN(BIT_NOT, "~"); }
"|" { BASIC_TOKEN(BIT_OR, "|"); }
"^" { BASIC_TOKEN(BIT_XOR, "^"); }
"," { BASIC_TOKEN(COMMA, ","); }

"#" { BASIC_TOKEN(POUND_SIGN, "#"); }
"->" { BASIC_TOKEN(ARROW, "->"); }

";" { BASIC_TOKEN(SEMICOLON, ";"); }
":" { BASIC_TOKEN(COLON, ":"); }
"=" { BASIC_TOKEN(ASSIGN, "="); }
">>" { BASIC_TOKEN(SHR_LOG, ">>"); }
"@>>" { BASIC_TOKEN(SHR_ARITH, "@>>"); }
"<<" { BASIC_TOKEN(SHL, "<<"); }

":[" { BASIC_TOKEN(UNSIGNED_CAST, ":["); }
"@:[" { BASIC_TOKEN(SIGNED_CAST, ":["); }

[0-9][A-Za-z_][A-Za-z0-9_]*|. {
    token = (Token){
        .type = UNPARSEABLE,
        .line = yylineno,
        ._should_free = true,
        .lexeme = current_lexeme(),
    };
    add_token(&builder, &token);
}
%%

#ifdef SCANNER_STANDALONE
int main(int argc, char **argv) {
    if (!argc) return EXIT_FAILURE;
    ++argv, --argc;
    yyin = argc ? fopen(argv[0], "r") : stdin;
    struct token_sequence tokens = yylex();
    for (size_t i = 0; i < tokens.len; ++i) {
#define TOK tokens.tokens[i]
        char *lexeme = checked_malloc(TOK.lexeme.len + 1);
        memcpy(lexeme, TOK.lexeme.text, TOK.lexeme.len);
        char *p;
        // replace NUL bytes with periods before printing
        while ((p = memchr(lexeme, 0, TOK.lexeme.len))) *p = '.';
        lexeme[TOK.lexeme.len] = '\0';

#define DEBUG_TOKENS
#include "lang/tokens.h"
        free(lexeme);
#undef TOK
    }
    if (argc) fclose(yyin);
    destroy_token_sequence(tokens);
    yylex_destroy();
}
#endif
