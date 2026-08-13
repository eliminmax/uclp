/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 * SPDX-FileCopyrightText: 2015 Robert Nystrom
 *
 * SPDX-License-Identifier: GPL-3.0-only AND MIT
 *
 * This parser borrows heavily from the clox `compiler.c` file from Nystrom's
 * Crafting Interpreters book.
 */

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <string.h>

#include "ast.h"
#include "lang/types.h"
#include "mem/arena.h"
#include "mem/checked.h"
#include "mem/dynarray.h"
#include "scanner.h"
#include "sized_string.h"
#include "string_manager.h"
#include "table.h"
#include "token.h"

#define USE [[clang::use_handle("parser")]]

// see https://stackoverflow.com/a/43788824
constexpr size_t MAX_SIZE_DIGITS = ((sizeof(size_t) * CHAR_BIT) * 28 / 93 + 2);

typedef struct parser *_Nonnull Parser;

struct var_scope {
    struct var_scope *_Nullable outer;
    HashTable vars;
};

struct parser {
    Scanner scanner;
    AllocGroup allocs;
    StringPool idents;
    HashTable types;
    struct var_scope current_vars;
    size_t ntokens;
    size_t token_cap;
    size_t next_tok_i;
    jmp_buf errbuf;
    Token *tokens [[clang::counted_by(ntokens)]];
};

typedef UCLExpression *_Nullable (*PrefixFn)(USE Parser, bool assignable);
typedef UCLExpression *_Nullable (*InfixFn)(
    USE Parser, bool assignable, UCLExpression *_Nonnull
);

static void *_Nonnull tracked_allocate(USE Parser, size_t sz) {
    return checked_malloc(sz);
}

#define ALLOCATE(p, t) tracked_allocate((p), sizeof(t))

[[gnu::cold, gnu::format(__printf__, 3, 4), noreturn]]
static void error_at(USE Parser p, ssize_t offset, const char *fmt, ...) {
    REQUIRE(offset >= 0 || (size_t)(-offset) < p->next_tok_i);

    Token t = p->tokens[p->next_tok_i - 1 + offset];
    fputs("Error at '", stderr);
    fwrite(t.lexeme.text, t.lexeme.len, 1, stderr);
    va_list fmt_args;
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    longjmp(p->errbuf, 1);
}

static UCLType *_Nonnull type_of_binding(
    USE Parser p, struct var_scope *_Nonnull scope, const String *ident
) {
    UCLType *type;
    if ((type = ht_lookup(scope->vars, *ident))) { return type; }
    if (scope->outer) return type_of_binding(p, scope->outer, ident);
    error_at(p, 0, "");
}

Alignment ucl_alignof(UCLType *_Nonnull type) {
    switch (type->tag) {
        case TYPE_OPAQUE: return type->opaque.alignment;
        case TYPE_I8: return ALIGN_8;
        case TYPE_I16: return ALIGN_16;
        case TYPE_I32: return ALIGN_32;
        case TYPE_I64: return ALIGN_64;
        case TYPE_POINTER: return ALIGN_PTR;
        case TYPE_ARRAY: return ucl_alignof(type->array.member_type);
    }
}

static void write_type(UCLType *type, CharBuffer *buf) {
    switch (type->tag) {
        case TYPE_POINTER:
            append_chars(buf, 1, u8"*");
            if (type->ptr.pointee_type) {
                write_type(type->ptr.pointee_type, buf);
            } else {
                append_chars(buf, 7, u8"dynamic");
            }
            break;
        case TYPE_OPAQUE: {
            struct ucl_opaque_type opaque = type->opaque;
            char numbuf[MAX_SIZE_DIGITS + 11];
            size_t sz =
                sprintf(numbuf, "opaque.%zu.%d", opaque.size, opaque.alignment);
            append_chars(buf, sz, (uchar *)numbuf);
            break;
        }
        case TYPE_I8: append_chars(buf, 2, u8"i8"); break;
        case TYPE_I16: append_chars(buf, 3, u8"i16"); break;
        case TYPE_I32: append_chars(buf, 3, u8"i32"); break;
        case TYPE_I64: append_chars(buf, 3, u8"i63"); break;
        case TYPE_ARRAY: {
            write_type(type->array.member_type, buf);
            char numbuf[MAX_SIZE_DIGITS + 3];
            size_t sz = sprintf(numbuf, "[%zu]", type->array.len);
            append_chars(buf, sz, (uchar *)numbuf);
        }
    }
}

// `number_at` is implemented with the knowledge that the TOKEN_INT_* enums are
// defined sequentially, first by base, then, within each base, in the order
// { <unsized>, 8, 16, 32, 64 }, and that the token passed is already known to
// be a valid numeric token.
//
// The following 8 asserts enusure that future refactors to the `SIZABLE_TOKEN`
// macro or the order of things in `lang/tokens.h` will maintain the invariants
// `number_at` relies on.
//
// Additionally, `is_literal` requires all literal identifiers to be contiguous,
// and these asserts are not repeated there.
static_assert(TOKEN_INT_BIN + 1 == TOKEN_INT_BIN_8);
static_assert(TOKEN_INT_BIN + 2 == TOKEN_INT_BIN_16);
static_assert(TOKEN_INT_BIN + 3 == TOKEN_INT_BIN_32);
static_assert(TOKEN_INT_BIN + 4 == TOKEN_INT_BIN_64);
static_assert(TOKEN_INT_BIN + 5 == TOKEN_INT_OCT);
static_assert(TOKEN_INT_OCT + 5 == TOKEN_INT_DEC);
static_assert(TOKEN_INT_DEC + 5 == TOKEN_INT_HEX);
static_assert(TOKEN_INT_HEX_64 - TOKEN_INT_BIN == 19);

[[gnu::const, gnu::artificial]] static inline int bits_to_index(int base) {
    switch (base) {
        case 8: return 0;
        case 16: return 1;
        case 32: return 2;
        case 64: return 3;
        default: unreachable();
    }
}

static uintmax_t number_at(USE Parser p, int offset, int fallback) {
    REQUIRE(fallback >= 0 && fallback < 4);
    Token t = p->tokens[p->next_tok_i - 1 - offset];
    // the one invariant that can't be statically asserted
    REQUIRE(t.type >= TOKEN_INT_BIN && t.type <= TOKEN_INT_HEX_64);
    char digits[66] = "";
    constexpr int BASES[4] = {2, 8, 10, 16};
    constexpr uintmax_t MAXES[4] = {
        UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX
    };

    int adjusted = t.type - TOKEN_INT_BIN;
    int base = BASES[adjusted / 5];
    adjusted = adjusted % 5 - 1;
    if (adjusted == -1) adjusted = bits_to_index(fallback);
    uintmax_t max = MAXES[adjusted % 5];
    size_t start = base == 10 ? 0 : 2;

    char *head = digits;
    char *const buf_end = &digits[64];
    for (size_t i = start; i < t.lexeme.len; i++) {
        if (t.lexeme.text[i] != u8'_') {
            if (head == buf_end) goto too_large;
            *head++ = t.lexeme.text[i];
        }
    }

    errno = 0;
    char *endptr;
    uintmax_t val = strtoumax((char *)t.lexeme.text, &endptr, base);
    if (errno == ERANGE || *endptr || val > max) goto too_large;

    return val;

too_large:
    error_at(p, offset, "");
}

static inline void init_type_map(USE Parser p) {
    struct common_type {
        String k;
        UCLType v;
    };

    const String KEYS[5] = {
        {.len = 2, .text = "i8"},
        {.len = 3, .text = "i16"},
        {.len = 3, .text = "i32"},
        {.len = 3, .text = "i64"},
        {.len = 8, .text = "*dynamic"},
    };

    const UCLType TYPES[5] = {
        {.tag = TYPE_I8},
        {.tag = TYPE_I16},
        {.tag = TYPE_I32},
        {.tag = TYPE_I64},
        {.tag = TYPE_POINTER, .ptr = {NULL, ALIGN_DYNAMIC}},
    };

    for (int i = 0; i < 5; i++) {
        UCLType *v =
            group_alloc(p->allocs, sizeof(UCLType *), alignof(UCLType *));
        memcpy(v, &TYPES[i], sizeof(UCLType));
        ht_set(p->types, KEYS[i], v);
    }
}

[[clang::acquire_handle("parser")]]
Parser parser(String source) {
    Parser p = checked_malloc(sizeof(struct parser));

    p->allocs = group_create();
    p->idents = new_pool(p->allocs);
    p->types = ht_create();

    p->scanner = start_scanner(source, p->idents);

    p->ntokens = 0;
    p->token_cap = 0;
    p->next_tok_i = 0;

    init_type_map(p);
    return p;
}

static void load_token(USE Parser p) {
    if (p->next_tok_i == p->ntokens) {
        if (p->ntokens == p->token_cap) {
            p->token_cap = new_capacity(p->token_cap);
            p->tokens =
                checked_reallocarray(p->tokens, p->token_cap, sizeof(Token));
        }
        Token t = p->tokens[++p->ntokens] = next_token(p->scanner);

        if (t.type == TOKEN_UNPARSEABLE) longjmp(p->errbuf, -1);
    }
}

static Token peek(USE Parser p) {
    load_token(p);
    return p->tokens[p->next_tok_i];
}

static Token advance(USE Parser p) {
    load_token(p);
    return p->tokens[p->next_tok_i++];
}

static Token current(USE Parser p) {
    REQUIRE(p->next_tok_i > 0);
    return p->tokens[p->next_tok_i - 1];
}

static bool advance_on(USE Parser p, TokenType type) {
    if (peek(p).type == type) {
        p->next_tok_i++;
        return true;
    }
    return false;
}

[[gnu::format_matches(__printf__, 3, "")]]
static Token consume(USE Parser p, enum token_type ty, const char *msg) {
    if (peek(p).type == ty) { return p->tokens[p->next_tok_i++]; }
    error_at(p, 1, msg);
}

static void free_type(UCLType *t) {
    switch (t->tag) {
        case TYPE_POINTER:
            if (t->ptr.pointee_type) free_type(t->ptr.pointee_type);
            break;
        case TYPE_ARRAY:
            if (t->array.member_type) free_type(t->array.member_type);
            break;
        default: break;
    }
    free(t);
}

static UCLType *read_type(USE Parser p) {
    Token tok;
    UCLType *type = NULL;
    switch ((tok = advance(p)).type) {
        case TOKEN_I8: return ht_lookup(p->types, (String){2, "i8"});
        case TOKEN_I16: return ht_lookup(p->types, (String){3, "i16"});
        case TOKEN_I32: return ht_lookup(p->types, (String){3, "i32"});
        case TOKEN_I64: return ht_lookup(p->types, (String){3, "i64"});
        case TOKEN_MUL: {
            if (peek(p).type == TOKEN_DYNAMIC) {
                return ht_lookup(p->types, (String){8, "*dynamic"});
            }
            type = checked_malloc(sizeof(UCLType));
            *type = (UCLType){
                .tag = TYPE_POINTER,
                .ptr = (struct ucl_pointer){
                    .pointee_type = read_type(p), .alignment = 0
                }
            };
            break;
        }
        default: error_at(p, 0, "Expected type");
    }

    while (peek(p).type == TOKEN_L_SQUARE) {
        advance(p);
        switch ((tok = advance(p)).type) {
            case TOKEN_INT_BIN:
            case TOKEN_INT_OCT:
            case TOKEN_INT_DEC:
            case TOKEN_INT_HEX:
                size_t len = number_at(p, 0, 64);
                UCLType *arr = checked_malloc(sizeof(UCLType));
                arr->tag = TYPE_ARRAY;
                arr->array.member_type = type;
                arr->array.len = len;
                type = arr;
                break;
            default:
                free(type);
                if (tok.type >= TOKEN_INT_BIN && tok.type <= TOKEN_INT_HEX_64) {
                    error_at(p, 0, "Array size can't have a size suffix");
                }
                error_at(p, 0, "Array size must be numeric literal");
        }
        if (!advance_on(p, TOKEN_R_SQUARE)) {
            free_type(type);
            error_at(p, 0, "Expect ']' after arrat size.");
        }
    }

    CharBuffer _buf = (CharBuffer){0, 0, NULL};
    write_type(type, &_buf);
    String name = (String){_buf.size, (char *)_buf.mem};
    UCLType *existing = ht_lookup(p->types, name);
    if (existing) {
        free_type(type);
        free(_buf.mem);
        return existing;
    }
    ht_set(p->types, name, type);
    return type;
}

enum expr_prec : uint8_t {
    XPREC_NONE,
    XPREC_ASSIGN, // = ~= += -= $%= etc.
    XPREC_LOGIC,  // && and ||
    XPREC_ORDER,  // != == < <= > >= $< $<= $> $>=
    XPREC_BSHIFT, // << $>> >>
    XPREC_BMASK,  //
    XPREC_TERM,   // + -
    XPREC_FACTOR, // * / % $* $/ $%
    XPREC_UNARY,  // ! ~ - :[N] $:[N] :{T} .* .&
    XPREC_CALL,   // () []
    XPREC_PRIMARY,
};

typedef struct {
    PrefixFn prefix;
    InfixFn infix;
    enum expr_prec precedence;
} ParseRule;

static UCLExpression *expression(USE Parser);
static UCLExpression *subexpr(USE Parser, enum expr_prec);

static UCLType *_Nonnull array_of(
    USE Parser p, size_t len, UCLType *_Nonnull mem_type
) {
    CharBuffer name = {};
    write_type(mem_type, &name);
    char numbuf[MAX_SIZE_DIGITS + 3];
    size_t sz = sprintf(numbuf, "[%zu]", len);
    append_chars(&name, sz, (uchar *)numbuf);
    String key = (String){name.size, (char *)name.mem};
    UCLType *t = ht_lookup(p->types, key);
    if (t) {
    } else {
        t = ALLOCATE(p, UCLType);
        t->tag = TYPE_ARRAY;
        t->array.len = len;
        t->array.member_type = mem_type;
        ht_set(p->types, key, t);
    }
    free(name.mem);
    return t;
}

static UCLExpression *_Nonnull array(USE Parser p, bool assignable) {
    size_t array_start = p->next_tok_i;
    if (peek(p).type == TOKEN_R_SQUARE) {
        error_at(p, 0, "Empty arrays are not supported");
    }

    size_t array_size = 1;
    size_t nest_level = 1;
    while (nest_level) {
        switch (advance(p).type) {
            case TOKEN_L_PAREN:
            case TOKEN_L_SQUARE: ++nest_level; break;
            case TOKEN_R_PAREN:
            case TOKEN_R_SQUARE: --nest_level; break;
            case TOKEN_COMMA:
                if (nest_level == 1) ++array_size;
                break;
            default: break;
        }
    }
    p->next_tok_i = array_start;

    struct array_definition *array = tracked_allocate(
        p, sizeof(struct array_definition) + sizeof(UCLExpression *[array_size])
    );
    array->size = array_size;
    array->vals[0] = expression(p);
    array->type = array->vals[0]->type;

    for (size_t i = 1; i < array_size; i++) {
        p->next_tok_i++; // skip past comma
        array->vals[i] = expression(p);
        if (array->vals[i]->type != array->type) {
            error_at(p, 0, "Mismatched types within array");
        }
    }

    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->tag = XPR_ARRAY;
    expr->array = array;
    expr->type = array_of(p, array_size, array->type);
    return expr;
}

#if CHAR_MIN < 0
#define CHAR_FIELD s
#else
#define CHAR_FIELD u
#endif
static ucl_i8 parse_char(const char *_Nonnull *const _Nonnull chr) {
    static_assert(UINT8_MAX == UCHAR_MAX);
    if (**chr == '\\') {
        switch (*++(*chr)) {
            case 'x': {
                uint8_t ret;
                *chr += sscanf(*chr, "x%2" SCNx8, &ret);
                return (ucl_i8){.u = ret};
            }
            case 'n': (*chr)++; return (ucl_i8){.u = u8'\n'};
            case 'r': (*chr)++; return (ucl_i8){.u = u8'\r'};
            case 't': (*chr)++; return (ucl_i8){.u = u8'\t'};
            default: {
                if (**chr >= '0' && **chr <= '9') {
                    uint8_t ret;
                    *chr += sscanf(*chr, "%" SCNo8, &ret);
                    return (ucl_i8){.u = ret};
                }
                // remaining escapes are \\, \", and \', so handle normally
                // having advanced
                break;
            }
        }
    }

    return (ucl_i8){.CHAR_FIELD = *(*chr)++};
}

#undef CHAR_FIELD

static bool is_assignment(TokenType ty) {
    switch (ty) {
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MOD_ASSIGN:
        case TOKEN_SIGNED_MUL_ASSIGN:
        case TOKEN_SIGNED_DIV_ASSIGN:
        case TOKEN_SIGNED_MOD_ASSIGN:
        case TOKEN_BIT_AND_ASSIGN:
        case TOKEN_BIT_OR_ASSIGN:
        case TOKEN_BIT_XOR_ASSIGN:
        case TOKEN_SHR_LOG_ASSIGN:
        case TOKEN_SHR_ARITH_ASSIGN:
        case TOKEN_SHL_ASSIGN:
        case TOKEN_ASSIGN: return true;
        default: return false;
    }
}

static UCLExpression *_Nonnull literal(USE Parser p, bool) {
    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->tag = XPR_LITERAL;
    TokenType ty;
    switch (ty = advance(p).type) {
        case TOKEN_CHAR: {
            expr->type = ht_lookup(p->types, (String){2, "i8"});
            assert(expr->type != NULL);
            const char *text = current(p).lexeme.text;
            expr->literal.tag = LIT_I8;
            expr->literal._i8 = parse_char(&text);
            assert(*text == '\'');
            return expr;
        }
        case TOKEN_STR: {
            String lexeme = current(p).lexeme;
            size_t len = lexeme.len - 2;
            expr->type =
                array_of(p, len, ht_lookup(p->types, (String){2, "i8"}));
            expr->literal.tag = LIT_STR;
            expr->literal.string = (String){len, &lexeme.text[1]};
            return expr;
        }
        default: {
            uintmax_t value = number_at(p, 0, 32);
            switch ((ty - TOKEN_INT_BIN) % 5) {
                case 1:
                    expr->literal.tag = LIT_I8;
                    expr->literal._i8.u = value;
                    return expr;
                case 2:
                    expr->literal.tag = LIT_I16;
                    expr->literal._i16.u = value;
                    return expr;
                case 0:
                case 3:
                    expr->literal.tag = LIT_I32;
                    expr->literal._i32.u = value;
                    return expr;
                case 4:
                    expr->literal.tag = LIT_I64;
                    expr->literal._i64.u = value;
                    return expr;
                default: UNREACHABLE("Modulo operation");
            }
        }
    }
}

static UCLExpression *_Nonnull grouping(USE Parser p, bool) {
    UCLExpression *grouped = expression(p);
    consume(p, TOKEN_R_PAREN, "Expected ')' after parenthesized expression");
    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->tag = XPR_GROUP;
    expr->grouped = grouped;
    return expr;
}

static UCLExpression *_Nonnull ident(USE Parser p, bool assignable) {
    Token tok = current(p);
    UCLExpression *e = ALLOCATE(p, UCLExpression);
    e->tag = XPR_VAR;
    e->var_name = intern(p->idents, tok.lexeme);
    e->type = type_of_binding(p, &p->current_vars, e->var_name);
    if (assignable && is_assignment(peek(p).type)) {
        p->next_tok_i++;
        UCLExpression *val = expression(p);
        UCLExpression *outer = ALLOCATE(p, UCLExpression);
        outer->tag = XPR_ASSIGN;
    }
    return e;
}

static bool is_numeric(UCLType *_Nonnull t) {
    switch (t->tag) {
        case TYPE_POINTER:
        case TYPE_OPAQUE:
        case TYPE_ARRAY: return false;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64: return true;
    }
}

static UCLExpression *_Nonnull index_(
    USE Parser p, bool assignable, UCLExpression *_Nonnull expr
) {
    do {
        if (expr->type->tag != TYPE_ARRAY) {
            error_at(p, 0, "Cannot index non-array expression");
        }
        UCLExpression *new_expr = ALLOCATE(p, UCLExpression);
        size_t saved_i = p->next_tok_i;
        new_expr->type = expr->type->array.member_type;
        new_expr->tag = XPR_INDEX;
        new_expr->index.array = expr;
        new_expr->index.index = expression(p);
        UCLExpression *index = expression(p);
        if (!is_numeric(index->type)) {
            p->next_tok_i = saved_i;
            error_at(p, 0, "Can't index an array with a non-numeric type");
        }
        expr = new_expr;
    } while (advance_on(p, TOKEN_L_SQUARE));

    return expr;
}

static UCLExpression *_Nonnull unary(USE Parser p, bool assignable) {
    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->tag = XPR_UNARY;
    expr->unary.op = current(p).type;
    expr->unary.operand = subexpr(p, XPREC_UNARY);
    return expr;
}

static UCLExpression *_Nonnull typecast(USE Parser p, bool) {
    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->tag = XPR_TYPECAST;
    expr->type = read_type(p);
    consume(p, TOKEN_R_CURLY, "Expect '}' after cast type");
    expr->type_cast.inner = subexpr(p, XPREC_UNARY);
    return expr;
}

static UCLExpression *_Nonnull call(
    USE Parser p, bool, UCLExpression *_Nonnull e
) {
    (void)e;
    (void)p;
    __builtin_trap();
}

static UCLExpression *_Nonnull binary(
    USE Parser p, bool, UCLExpression *_Nonnull e
) {
    (void)e;
    (void)p;
    __builtin_trap();
}

static UCLExpression *_Nonnull assign(
    USE Parser p, bool, UCLExpression *_Nonnull e
) {
    (void)e;
    (void)p;
    __builtin_trap();
}

static UCLExpression *_Nonnull bitcast(USE Parser p, bool) {
    (void)p;
    __builtin_trap();
}

// clang-format off

// Pratt parsing rule table
static const ParseRule EXPR_RULES[TOKEN_EOF + 1] = {
    [TOKEN_L_SQUARE]            = {array,       index_,  XPREC_CALL},
    [TOKEN_L_PAREN]             = {grouping,    call,   XPREC_CALL},
    [TOKEN_PLUS]                = {NULL,        binary, XPREC_TERM},
    [TOKEN_MINUS]               = {unary,       binary, XPREC_TERM},
    [TOKEN_MUL]                 = {unary,       binary, XPREC_FACTOR},
    [TOKEN_DIV]                 = {NULL,        binary, XPREC_FACTOR},
    [TOKEN_MOD]                 = {NULL,        binary, XPREC_FACTOR},
    [TOKEN_SIGNED_MUL]          = {NULL,        binary, XPREC_FACTOR},
    [TOKEN_SIGNED_DIV]          = {NULL,        binary, XPREC_FACTOR},
    [TOKEN_SIGNED_MOD]          = {NULL,        binary, XPREC_FACTOR},
    [TOKEN_PLUS_ASSIGN]         = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_MINUS_ASSIGN]        = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_MUL_ASSIGN]          = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_DIV_ASSIGN]          = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_MOD_ASSIGN]          = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SIGNED_MUL_ASSIGN]   = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SIGNED_DIV_ASSIGN]   = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SIGNED_MOD_ASSIGN]   = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_EQ]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_NE]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_LT]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_GT]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_LE]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_GE]                  = {NULL,        binary, XPREC_ORDER},
    [TOKEN_SIGNED_LT]           = {NULL,        binary, XPREC_ORDER},
    [TOKEN_SIGNED_GT]           = {NULL,        binary, XPREC_ORDER},
    [TOKEN_SIGNED_LE]           = {NULL,        binary, XPREC_ORDER},
    [TOKEN_SIGNED_GE]           = {NULL,        binary, XPREC_ORDER},
    [TOKEN_DEREF]               = {unary,       NULL,   XPREC_NONE},
    [TOKEN_REF]                 = {unary,       NULL,   XPREC_NONE},
    [TOKEN_LOGICAL_NOT]         = {NULL,        binary, XPREC_LOGIC},
    [TOKEN_LOGICAL_AND]         = {NULL,        binary, XPREC_LOGIC},
    [TOKEN_LOGICAL_OR]          = {NULL,        binary, XPREC_LOGIC},
    [TOKEN_BIT_NOT]             = {NULL,        binary, XPREC_BMASK},
    [TOKEN_BIT_AND]             = {NULL,        binary, XPREC_BMASK},
    [TOKEN_BIT_OR]              = {NULL,        binary, XPREC_BMASK},
    [TOKEN_BIT_XOR]             = {NULL,        binary, XPREC_BMASK},
    [TOKEN_BIT_AND_ASSIGN]      = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_BIT_OR_ASSIGN]       = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_BIT_XOR_ASSIGN]      = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SHR_LOG]             = {NULL,        binary, XPREC_BSHIFT},
    [TOKEN_SHR_ARITH]           = {NULL,        binary, XPREC_BSHIFT},
    [TOKEN_SHL]                 = {NULL,        binary, XPREC_BSHIFT},
    [TOKEN_SHR_LOG_ASSIGN]      = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SHR_ARITH_ASSIGN]    = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_SHL_ASSIGN]          = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_ASSIGN]              = {NULL,        assign, XPREC_ASSIGN},
    [TOKEN_UNSIGNED_BITCAST_8]  = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_16] = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_32] = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_64] = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_8]    = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_16]   = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_32]   = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_64]   = {bitcast,     NULL,   XPREC_NONE},
    [TOKEN_TYPE_CAST]           = {typecast,    NULL,   XPREC_NONE},
    [TOKEN_OPAQUE_8]            = {literal,     NULL,   XPREC_NONE},
    [TOKEN_OPAQUE_16]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_OPAQUE_32]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_OPAQUE_64]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_WHILE]               = {literal,     NULL,   XPREC_NONE},
    [TOKEN_STR]                 = {literal,     NULL,   XPREC_NONE},
    [TOKEN_CHAR]                = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_DEC]             = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_DEC_8]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_DEC_16]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_DEC_32]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_DEC_64]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_BIN]             = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_BIN_8]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_BIN_16]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_BIN_32]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_BIN_64]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_OCT]             = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_OCT_8]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_OCT_16]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_OCT_32]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_OCT_64]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_HEX]             = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_HEX_8]           = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_HEX_16]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_HEX_32]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_INT_HEX_64]          = {literal,     NULL,   XPREC_NONE},
    [TOKEN_IDENT]               = {ident,       NULL,   XPREC_NONE},
};
// clang-format on

static UCLExpression *subexpr(USE Parser p, enum expr_prec prec) {
    PrefixFn pref;
    if (!(pref = EXPR_RULES[advance(p).type].prefix)) {}
    __builtin_trap();
}

static UCLExpression *expression(USE Parser p) {
    (void)p;
    __builtin_trap();
}

[[gnu::cold]]
static int ident_printf_len(const String *s) {
    if (s->len > 80) [[clang::unlikely]] { return 80; }
    return s->len;
}

static Statement var_decl(USE Parser p, Placement plcmnt) {
    size_t start = p->next_tok_i - 1;
    Statement stmt = {.tag = STMT_VAR_DEC};
    stmt.declaration.ident = intern(
        p->idents, consume(p, TOKEN_IDENT, "Expect variable name").lexeme
    );
    stmt.declaration.placement = plcmnt;
    bool type_known = false;
    UCLType *type = NULL;
    if (peek(p).type == TOKEN_COLON) {
        type_known = true;
        type = read_type(p);
    }

    if (peek(p).type == TOKEN_ASSIGN) {
        p->next_tok_i++;
        stmt.declaration.value = expression(p);
        if (type_known) {
            if (stmt.declaration.value->type != type) {
                error_at(p, 0, "Type mismatch");
            }
        }
        type_known = true;
    } else {
        stmt.declaration.value = NULL;
    }

    if (!type_known) {
        int offset;
        if (ckd_sub(&offset, p->next_tok_i, start)) [[clang::unlikely]] {
            offset = 1;
        }
        error_at(
            p,
            offset,
            "Must provide either an explicit type and/or a value for %.*s",
            ident_printf_len(stmt.declaration.ident),
            stmt.declaration.ident->text
        );
    }

    consume(p, TOKEN_SEMICOLON, "Expect ';' after declaration");
    return stmt;
}

static Statement block_stmt(USE Parser p) {
    (void)p;
    __builtin_trap();
}

static Statement cond_stmt(USE Parser p) {
    (void)p;
    __builtin_trap();
}

static Statement expr_stmt(USE Parser p) {
    (void)p;
    __builtin_trap();
}

static Statement while_stmt(USE Parser p) {
    (void)p;
    __builtin_trap();
}

static Statement foreach_stmt(USE Parser p) {
    (void)p;
    __builtin_trap();
}

const bool IS_ASSIGNMENT[TOKEN_EOF + 1] = {
    [TOKEN_ASSIGN] = true,
    [TOKEN_PLUS_ASSIGN] = true,
    [TOKEN_MINUS_ASSIGN] = true,
    [TOKEN_MUL_ASSIGN] = true,
    [TOKEN_DIV_ASSIGN] = true,
    [TOKEN_MOD_ASSIGN] = true,
    [TOKEN_SIGNED_MUL_ASSIGN] = true,
    [TOKEN_SIGNED_DIV_ASSIGN] = true,
    [TOKEN_SIGNED_MOD_ASSIGN] = true,
    [TOKEN_BIT_AND_ASSIGN] = true,
    [TOKEN_BIT_OR_ASSIGN] = true,
    [TOKEN_BIT_XOR_ASSIGN] = true,
    [TOKEN_SHR_LOG_ASSIGN] = true,
    [TOKEN_SHR_ARITH_ASSIGN] = true,
    [TOKEN_SHL_ASSIGN] = true,
};

Statement get_statement(USE Parser p) {
    if (setjmp(p->errbuf)) return (Statement){.tag = STMT_UNPARSABLE};
    Token t;

    switch ((t = advance(p)).type) {
        case TOKEN_LET: return var_decl(p, STORE_STACK);
        case TOKEN_STATIC: return var_decl(p, STORE_STATIC);
        case TOKEN_IF: return cond_stmt(p);
        case TOKEN_L_CURLY: return block_stmt(p);
        case TOKEN_WHILE: return while_stmt(p);
        case TOKEN_FOREACH: return foreach_stmt(p);
        default: return expr_stmt(p);
    }
}
