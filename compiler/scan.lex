%{
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <string.h>

#include "sized_string.h"
#include "token.h"

#define YY_DECL struct token_sequence yylex(void)
#define yyterminate() return builder.sequence

struct token_sequence_builder {
    struct token_sequence sequence;
    size_t capacity;
};

extern inline void destroy_token(Token);

static bool add_token(
    struct token_sequence_builder *builder, Token *token
) {
    assert(builder->sequence.ntokens <= builder->capacity);
    assert(builder->sequence.ntokens < SIZE_MAX / 2);
    if (builder->sequence.ntokens == builder->capacity) {
        size_t new_capacity = builder->sequence.ntokens * 2;
        if (new_capacity >= SIZE_MAX / sizeof(Token)) return false;
        Token *new_tokens =
            realloc(builder->sequence.tokens, new_capacity * sizeof(Token));
        if (!new_tokens) return false;
        builder->capacity = new_capacity;
        builder->sequence.tokens = new_tokens;
    }

    memcpy(
        &builder->sequence.tokens[builder->sequence.ntokens],
        token,
        sizeof(Token)
    );
    builder->sequence.ntokens++;
#ifdef SCANNER_STANDALONE
    printf("%s -> %" PRIu8 "\n", yytext, (uint8_t)token->type);
#endif
    return true;
}
#define COMMON_STR(s) (String){.text = s, .len = sizeof(s) - 1 }

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
    String str = {.text = checked_malloc(yyleng), .len = yyleng};
    memcpy(str.text, yytext, yyleng);
    return str;
}

static void parse_int(Token *token, const char *start, int base) {
    char *end;
    token->_should_free = true;
    token->lexeme = current_lexeme();
    token->line = yylineno;
    unsigned long long val = strtoull(start, &end, base);
    if (*end == '#') {
        switch (end[1]) {
            case '8':
                assert(val <= UINT8_MAX);
                token->type = LITERAL_INT8, token->literal_i8.u = val;
                break;
            case '1':
                assert(val <= UINT16_MAX && end[2] == '6');
                token->type = LITERAL_INT16, token->literal_i16.u = val;
                break;
            case '3':
                assert(val <= UINT32_MAX && end[2] == '2');
                token->type = LITERAL_INT32, token->literal_i32.u = val;
                break;
            case '6':
                assert(val <= UINT64_MAX && end[2] == '4');
                token->type = LITERAL_INT64, token->literal_i64.u = val;
                break;
        }
    } else {
        assert(val <= UINT32_MAX);
        token->type = LITERAL_INT32, token->literal_i32.u = val;
    }
}

%}

%option noyywrap
%option yylineno
%option nodefault
%pointer
whitespace [ \n\t]+

char_literal '([^'\\"]|\\[ntr]|\\x[0-9a-f]{2}|\[0-9]{1,3})'

string_literal \"([^"]|\\\")*\"
int_size_suffix (#8|#16|#32|#64)?
dec_literal [0-9]+{int_size_suffix}
bin_literal 0b[01]+{int_size_suffix}
oct_literal 0o[0-7]+{int_size_suffix}
hex_literal 0x[0-9a-fA-F]+{int_size_suffix}
ident [_a-zA-Z][_0-9a-zA-Z]*

cast_8 :{whitespace}?\[{whitespace}?8{whitespace}?\]
cast_16 :{whitespace}?\[{whitespace}?16{whitespace}?\]
cast_32 :{whitespace}?\[{whitespace}?32{whitespace}?\]
cast_64 :{whitespace}?\[{whitespace}?64{whitespace}?\]

multiline_comment_body ([^*]|[*]/[^/])*

comment ([/]{2}.*|\/\*.*\*\/)

%%
    #define BASIC_TOKEN(t, l) \
        token = (Token) {.lexeme = COMMON_STR(l), .type = t, .line = yylineno }; \
        add_token(&builder, &token);
    struct token_sequence_builder builder = (struct token_sequence_builder){
        .sequence = (struct token_sequence){
            .tokens = checked_malloc(sizeof(Token)),
            .ntokens = 0
        },
        .capacity = 1
    };
    Token token;

{whitespace}

func { BASIC_TOKEN(KW_FUNC, "func"); }
let { BASIC_TOKEN(KW_LET, "let"); }
if { BASIC_TOKEN(KW_IF, "if"); }
elif { BASIC_TOKEN(KW_ELIF, "elif"); }
else { BASIC_TOKEN(KW_ELSE, "else"); }
while { BASIC_TOKEN(KW_WHILE, "while"); }
i8 { BASIC_TOKEN(KW_I8, "i8"); }
i16 { BASIC_TOKEN(KW_I16, "i16"); }
i32 { BASIC_TOKEN(KW_I32, "i32"); }
i64 { BASIC_TOKEN(KW_I64, "i64"); }

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
    if (*yytext == '\\') {
        switch(yytext[1]) {
            case 'n': val = '\n'; break;
            case 'r': val = '\r'; break;
            case 't': val = '\t'; break;
            case 'x':
                val = strtoull(&yytext[2], NULL, 16);
                break;
            case '\\':
            case '\"':
            case '\'':
                val = yytext[1];
                break;
            default:
                assert(isdigit(yytext[1]));
                val = strtoull(&yytext[2], NULL, 8);
        }
    } else {
        val = *yytext;
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

"&&" { BASIC_TOKEN(LOG_AND, "&&"); }
"||" { BASIC_TOKEN(LOG_OR, "||"); }

"&" { BASIC_TOKEN(BIT_AND, "&"); }
"~" { BASIC_TOKEN(BIT_NOT, "~"); }
"|" { BASIC_TOKEN(BIT_OR, "|"); }
"^" { BASIC_TOKEN(BIT_XOR, "^"); }
"," { BASIC_TOKEN(COMMA, ","); }

"->" { BASIC_TOKEN(ARROW, "->"); }

";" { BASIC_TOKEN(SEMICOLON, ";"); }
"=" { BASIC_TOKEN(EQ_ASSIGN, "="); }
">>" { BASIC_TOKEN(SHR_LOG, ">>"); }
"@>>" { BASIC_TOKEN(SHR_ARITH, "@>>"); }
"<<" { BASIC_TOKEN(SHL, "<<"); }
. { fprintf(stderr, "Error lexing \"%s\".\n", yytext); exit(EXIT_FAILURE); }
%%

#ifdef SCANNER_STANDALONE
int main(int argc, char **argv) {
    if (!argc) return EXIT_FAILURE;
    ++argv, --argc;
    yyin = argc ? fopen(argv[0], "r") : stdin;
    yylex();
}
#endif
