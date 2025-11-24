/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

%option noyywrap yylineno nodefault reentrant
%pointer

%{
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sized_string.h"
#include "token.h"
#include "alloc.h"

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
        &builder->sequence.tokens[builder->sequence.len], token, sizeof(Token)
    );
    builder->sequence.len++;
}

// return a heap-allocated String containing a copy of the current text match
static String current_lexeme(yyscan_t scanner);

// shrink the builder's allocation down.
static void finalize(struct token_sequence_builder *builder) {
    if (builder->sequence.len == 0) {
        add_token(
            builder,
            &(Token){
                .lexeme = {.text = "", .len = 0},
                .type = TOKEN_UNPARSEABLE,
                .line = 0,
            }
        );
    }
    Token *addr = reallocarray(
        builder->sequence.tokens, builder->sequence.len, sizeof(Token)
    );
    if (!addr) {
        perror("Failed to shrink allocation of token sequence");
        abort();
    }
    builder->capacity = builder->sequence.len;
}
#define YY_DECL \
    void yylex(struct token_sequence_builder *builder, yyscan_t yyscanner)
#define yyterminate() \
    finalize(builder); \
    return
%}

whitespace [ \n\t]+

escaped_char \\[ntr'\"\\]|\\x[0-9a-f]{2}|\\[0-3]?([0-7]{1,2})
char_literal '([^'\\\n\r]|{escaped_char})'
string_literal \"([^"\\\n\r]|{escaped_char})*\"

int_size_suffix (#[0-9]+)?
dec_literal [0-9]+{int_size_suffix}
bin_literal 0b[01]+{int_size_suffix}
oct_literal 0o[0-7]+{int_size_suffix}
hex_literal 0x[0-9a-fA-F]+{int_size_suffix}
ident [_a-zA-Z][_0-9a-zA-Z]*

%%
    #define FIXED_TOKEN(t, l) \
        add_token(builder, &(Token){ \
            .lexeme = (String){.text = l, .len = sizeof(l) - 1}, \
            .type = TOKEN_ ## t, \
            .line = yylineno, \
        });
    #define VALUE_TOKEN(t) \
        add_token(builder, &(Token){ \
            .lexeme = current_lexeme(yyscanner), \
            .type = TOKEN_ ## t, \
            .line = yylineno, \
            ._should_free = true, \
        });


[ \n\t]+
"//".*
"/*"([^/]|[^*][/])*"*/"

"let" { FIXED_TOKEN(KW_LET, "let"); }
"static" { FIXED_TOKEN(KW_STATIC, "static"); }
"func" { FIXED_TOKEN(KW_FUNC, "func"); }
"if" { FIXED_TOKEN(KW_IF, "if"); }
"elif" { FIXED_TOKEN(KW_ELIF, "elif"); }
"else" { FIXED_TOKEN(KW_ELSE, "else"); }
"while" { FIXED_TOKEN(KW_WHILE, "while"); }
"dynamic" { FIXED_TOKEN(KW_DYNAMIC, "dynamic"); }
"opaque" { FIXED_TOKEN(KW_OPAQUE, "opaque"); }
"i8" { FIXED_TOKEN(KW_I8, "i8"); }
"i16" { FIXED_TOKEN(KW_I16, "i16"); }
"i32" { FIXED_TOKEN(KW_I32, "i32"); }
"i64" { FIXED_TOKEN(KW_I64, "i64"); }

{ident} { VALUE_TOKEN(IDENT); }

{char_literal} {
    VALUE_TOKEN(LITERAL_CHAR); }
{string_literal} {
    VALUE_TOKEN(LITERAL_STR); }

{dec_literal} { VALUE_TOKEN(LITERAL_INT_DEC); }
{hex_literal} { VALUE_TOKEN(LITERAL_INT_HEX); }
{oct_literal} { VALUE_TOKEN(LITERAL_INT_OCT); }
{bin_literal} { VALUE_TOKEN(LITERAL_INT_BIN); }

"[" { FIXED_TOKEN(L_SQUARE, "["); }
"]" { FIXED_TOKEN(R_SQUARE, "]"); }
"{" { FIXED_TOKEN(L_CURLY, "{"); }
"}" { FIXED_TOKEN(R_CURLY, "}"); }
"(" { FIXED_TOKEN(L_PAREN, "("); }
")" { FIXED_TOKEN(R_PAREN, ")"); }

"+" { FIXED_TOKEN(ARITH_PLUS, "+"); }
"-"  { FIXED_TOKEN(ARITH_MINUS, "-"); }
"*" { FIXED_TOKEN(ARITH_MUL, "*"); }
"/" { FIXED_TOKEN(ARITH_DIV, "/"); }
"%" { FIXED_TOKEN(ARITH_MOD, "%"); }
"@*" { FIXED_TOKEN(ARITH_SIGNED_MUL, "@*"); }
"@/" { FIXED_TOKEN(ARITH_SIGNED_DIV, "@/"); }
"@%" { FIXED_TOKEN(ARITH_SIGNED_MOD, "@%"); }

"==" { FIXED_TOKEN(CMP_EQ, "=="); }
"!=" { FIXED_TOKEN(CMP_NE, "!="); }
">" { FIXED_TOKEN(CMP_GT, ">"); }
"<" { FIXED_TOKEN(CMP_LT, "<"); }
">=" { FIXED_TOKEN(CMP_GE, ">="); }
"<=" { FIXED_TOKEN(CMP_LE, "<="); }
"@>" { FIXED_TOKEN(CMP_SIGNED_GT, "@>"); }
"@<" { FIXED_TOKEN(CMP_SIGNED_LT, "@<"); }
"@>=" { FIXED_TOKEN(CMP_SIGNED_GE, "@>="); }
"@<=" { FIXED_TOKEN(CMP_SIGNED_LE, "@<="); }

".*" { FIXED_TOKEN(DEREF, ".*"); }
".&" { FIXED_TOKEN(REF, ".&"); }

"&&" { FIXED_TOKEN(LOGICAL_AND, "&&"); }
"||" { FIXED_TOKEN(LOGICAL_OR, "||"); }

"&" { FIXED_TOKEN(BIT_AND, "&"); }
"~" { FIXED_TOKEN(BIT_NOT, "~"); }
"|" { FIXED_TOKEN(BIT_OR, "|"); }
"^" { FIXED_TOKEN(BIT_XOR, "^"); }
"," { FIXED_TOKEN(COMMA, ","); }

"#" { FIXED_TOKEN(POUND_SIGN, "#"); }
"->" { FIXED_TOKEN(ARROW, "->"); }

";" { FIXED_TOKEN(SEMICOLON, ";"); }
":" { FIXED_TOKEN(COLON, ":"); }
"=" { FIXED_TOKEN(ASSIGN, "="); }
">>" { FIXED_TOKEN(SHR_LOG, ">>"); }
"@>>" { FIXED_TOKEN(SHR_ARITH, "@>>"); }
"<<" { FIXED_TOKEN(SHL, "<<"); }

":[" { FIXED_TOKEN(UNSIGNED_CAST, ":["); }
"@:[" { FIXED_TOKEN(SIGNED_CAST, "@:["); }

[0-9][A-Za-z_][A-Za-z0-9_]*|. { VALUE_TOKEN(UNPARSEABLE); }
%%

#ifdef SCANNER_STANDALONE
int main(int argc, char **argv) {
    if (!argc) return EXIT_FAILURE;
    ++argv, --argc;
    yyscan_t scanner;
    yylex_init(&scanner);

    FILE *in;
    if (argc) {
        in = fopen(argv[0], "r");
        if (!in) {
            perror("Failed to open file");
            return EXIT_FAILURE;
        }
        yyset_in(in, scanner);
    }
    struct token_sequence_builder builder = (struct token_sequence_builder){
        .sequence =
            (struct token_sequence){
                .len = 0,
                .tokens = checked_malloc(sizeof(Token)),
            },
        .capacity = 1
    };
    yylex(&builder, scanner);
    if (argc) fclose(in);
    yylex_destroy(scanner);

    for (size_t i = 0; i < builder.sequence.len; ++i) {
#define TOK builder.sequence.tokens[i]
        char *lexeme = checked_malloc(TOK.lexeme.len + 1);
        memcpy(lexeme, TOK.lexeme.text, TOK.lexeme.len);
        char *p;
        // replace NUL bytes with periods before printing
        while ((p = memchr(lexeme, 0, TOK.lexeme.len))) *p = '.';
        lexeme[TOK.lexeme.len] = '\0';

        // if included here after defining DEBUG_TOKENS, this header will match
        // the valid tag values for tokens to the proper invocation of printf to
        // print representations of their values
#define DEBUG_TOKENS
#include "lang/tokens.h"
#undef TOK
        free(lexeme);
    }
    destroy_token_sequence(builder.sequence);
}

#else

struct token_sequence tokenize(size_t len, const char source[_Nonnull len]) {
    yyscan_t scanner;
    yylex_init(&scanner);
    if (len > INT_MAX - 2) {
        fputs("Source buffer too large for lex-generated parser\n", stderr);
        exit(EXIT_FAILURE);
    }
    yy_scan_bytes(source, len, scanner);

    struct token_sequence_builder builder = (struct token_sequence_builder){
        .sequence =
            (struct token_sequence){
                .len = 0,
                .tokens = checked_malloc(sizeof(Token)),
            },
        .capacity = 1
    };

    yylex_destroy(scanner);
    return builder.sequence;
}
#endif

static String current_lexeme(yyscan_t scanner) {
    int len = yyget_leng(scanner);
    assert(len >= 0);

    String str = (String){
        .len = len,
        .text = memcpy(checked_malloc(len), yyget_text(scanner), len),
    };
    return str;
}
