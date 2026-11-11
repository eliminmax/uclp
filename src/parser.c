/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 * SPDX-FileCopyrightText: 2015 Robert Nystrom
 *
 * SPDX-License-Identifier: GPL-3.0-only AND MIT
 *
 * This parser borrows heavily from the clox `compiler.c` file from Nystrom's
 * Crafting Interpreters book.
 */

#include "parser.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <string.h>
#include <unistd.h>

#include "annotations.h"
#include "ast.h"
#include "lang/types.h"
#include "mem/arena.h"
#include "mem/checked.h"
#include "mem/dynarray.h"
#include "printerr.h"
#include "scanner.h"
#include "sized_string.h"
#include "string_manager.h"
#include "table.h"
#include "todo.h"
#include "token.h"
#include "typeck.h"

#define USE [[clang::use_handle("parser")]]
// Approximation of digits in SIZE_MAX; see https://stackoverflow.com/a/43788824
constexpr size_t MAX_SIZE_DIGITS = ((sizeof(size_t) * CHAR_BIT) * 28 / 93 + 2);

typedef UCLExpression *_Nullable (*PrefixFn)(USE Parser);
typedef UCLExpression *_Nullable (*InfixFn)(
    USE Parser, UCLExpression *_Nonnull
);

struct var_scope {
    struct var_scope *_Nullable outer;
    HashTable vars;
};

struct parser {
    Scanner scanner;
    AllocGroup ag;
    StringPool idents;
    HashTable types;

    struct {
        size_t count;
        size_t cap;
        void **ptrs;
    } build_allocs;
    struct var_scope current_vars;
    size_t ntokens;
    size_t token_cap;
    size_t next_tok_i;
    Token *tokens [[clang::counted_by(ntokens)]];
    bool panicking;
};

static void *_Nonnull tracked_alloc(USE Parser p, size_t sz) {
    if (p->build_allocs.count == p->build_allocs.cap) {
        p->build_allocs.cap = new_capacity(p->build_allocs.cap);
        p->build_allocs.ptrs = checked_reallocarray(
            p->build_allocs.ptrs, p->build_allocs.cap, sizeof(void **)
        );
    }
    return p->build_allocs.ptrs[p->build_allocs.count++] = checked_malloc(sz);
}

#define ALLOCATE(p, t) tracked_alloc((p), sizeof(t))

// helper to allocate new expressions, which explicitly sets the type to NULL
[[gnu::always_inline, gnu::artificial]] static inline UCLExpression
    *_Nonnull alloc_expr(USE Parser p) {
    UCLExpression *expr = ALLOCATE(p, UCLExpression);
    expr->type = NULL;
    return expr;
}

[[gnu::cold, gnu::format(__printf__, 3, 4)]]
static void error_at(USE Parser p, ssize_t offset, const char *fmt, ...) {
    REQUIRE(offset >= 0 || (size_t)(-offset) < p->next_tok_i);
    if (p->panicking) return;
    p->panicking = true;
    Token t = p->tokens[p->next_tok_i - 1 + offset];

    if (t.type == TOKEN_UNPARSEABLE) return;
    va_list fmt_args;
    va_start(fmt_args, fmt);

    String source = get_source(p->scanner);
    ucl_verror(
        t.line,
        (uchar *)source.text,
        source.len,
        (uchar *)t.lexeme.text,
        t.lexeme.len,
        fmt,
        fmt_args
    );
    va_end(fmt_args);
}

UCLType *_Nullable type_of_binding(
    USE Parser p, struct var_scope *_Nonnull scope, const String *ident
) {
    UCLType *type;
    if ((type = ht_lookup(scope->vars, *ident))) { return type; }
    if (scope->outer) return type_of_binding(p, scope->outer, ident);
    return NULL;
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
    REQUIRE(
        fallback == 8 || fallback == 16 || fallback == 32 || fallback == 64
    );
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
    char *endptr = NULL;
    uintmax_t val = strtoumax((char *)t.lexeme.text, &endptr, base);
    if (errno == ERANGE || val > max) goto too_large;

    return val;

too_large:
    if (t.lexeme.len > 0x1000) {
        error_at(p, offset, "too large: '%4093s...'", t.lexeme.text);
        return -1;
    } else {
        error_at(
            p, offset, "too large: '%.*s'", (int)t.lexeme.len, t.lexeme.text
        );
        return -1;
    }
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
        {.tag = TYPE_POINTER, .pointee_type = NULL},
    };

    for (int i = 0; i < 5; i++) {
        UCLType *v = group_alloc(p->ag, sizeof(UCLType), alignof(UCLType));
        memcpy(v, &TYPES[i], sizeof(UCLType));
        ht_set(p->types, KEYS[i], v);
    }
}

static void load_token(USE Parser p) {
    if (p->next_tok_i == p->ntokens) {
        if (p->ntokens == p->token_cap) {
            p->token_cap = new_capacity(p->token_cap);
            p->tokens =
                checked_reallocarray(p->tokens, p->token_cap, sizeof(Token));
        }
        Token t = p->tokens[p->ntokens++] = next_token(p->scanner);

        if (t.type == TOKEN_UNPARSEABLE) p->panicking = true;
    }
}

[[clang::acquire_handle("parser")]]
Parser parser(String source) {
    Parser p = checked_malloc(sizeof(struct parser));

    AllocGroup ag = group_create();
    StringPool idents = new_pool(ag);
    HashTable types = ht_create();
    Scanner scanner = start_scanner(source, idents);
    struct var_scope current_vars = {.vars = ht_create(), .outer = NULL};

    // default initialization rules initialize all unspecified fields if
    // designated initializer syntax is used
    *p = (struct parser){
        .ag = ag,
        .idents = idents,
        .types = types,
        .scanner = scanner,
        .next_tok_i = 0,
        .ntokens = 1,
        .token_cap = 8,
        .current_vars = current_vars,
        .tokens = checked_malloc(sizeof(Token[8])),
    };

    init_type_map(p);

    p->tokens[0] = next_token(p->scanner);
    return p;
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

struct [[deprecated]] consume_result {
    bool ok;
};

[[nodiscard, gnu::format_matches(__printf__, 3, "")]]
static bool consume(USE Parser p, enum token_type ty, const char *msg) {
    if (advance_on(p, ty)) [[clang::likely]] { return true; }
    error_at(p, 1, msg);
    return false;
}

extern inline String as_string(CharBuffer);

static UCLType *read_type(USE Parser p) {
    UCLType *type = NULL;
    CharBuffer name_buf = (CharBuffer){0, 0, NULL};
    bool new_alloc = false;
    switch (advance(p).type) {
        case TOKEN_I8: type = ht_lookup(p->types, (String){2, "i8"}); break;
        case TOKEN_I16: type = ht_lookup(p->types, (String){3, "i16"}); break;
        case TOKEN_I32: type = ht_lookup(p->types, (String){3, "i32"}); break;
        case TOKEN_I64: type = ht_lookup(p->types, (String){3, "i64"}); break;
        case TOKEN_DEREF:
        case TOKEN_MUL: {
            if (advance_on(p, TOKEN_DYNAMIC)) {
                type = ht_lookup(p->types, (String){8, "*dynamic"});
            } else {
                UCLType *pointee_type = read_type(p);
                if (pointee_type == NULL) {
                    return NULL;
                }
                new_alloc = true;
                type = checked_malloc(sizeof(UCLType));
                type->tag = TYPE_POINTER;
                type->pointee_type = pointee_type;
            }
            break;
        }
        default: error_at(p, 0, "Expected type"); return NULL;
    }

    write_type(type, &name_buf);

    UCLType *cached = ht_lookup(p->types, as_string(name_buf));

    if (cached) {
        if (new_alloc) {
            free(type);
            new_alloc = false;
        }
        type = cached;
    } else {
        group_claim(p->ag, type);
    }

    Token tok;
    while (advance_on(p, TOKEN_L_SQUARE)) {
        switch ((tok = advance(p)).type) {
            case TOKEN_INT_BIN:
            case TOKEN_INT_OCT:
            case TOKEN_INT_DEC:
            case TOKEN_INT_HEX:
                size_t len = number_at(p, 0, 64);
                UCLType *arr = checked_malloc(sizeof(UCLType));
                new_alloc = true;
                arr->tag = TYPE_ARRAY;
                arr->array.member_type = type;
                arr->array.len = len;
                type = arr;
                break;
            default:

                if (tok.type >= TOKEN_INT_BIN && tok.type <= TOKEN_INT_HEX_64) {
                    if ((tok.type - TOKEN_INT_BIN) % 5) {
                        error_at(p, 0, "Array size can't have a size suffix");
                    }
                } else {
                    error_at(p, 0, "Array size must be numeric literal");
                }
                if (new_alloc) {
                    free(type);
                    new_alloc = false;
                }
                return NULL;
        }

        if (!advance_on(p, TOKEN_R_SQUARE)) {
            if (new_alloc) {
                free(type);
                new_alloc = false;
            }
            error_at(p, 0, "Expect ']' after array size.");
            return NULL;
        }

        char suf_buf[MAX_SIZE_DIGITS + 3] = {};
        size_t sz = sprintf(suf_buf, "[%zu]", type->array.len);
        append_chars(&name_buf, sz, (uchar *)suf_buf);
        UCLType *existing = ht_lookup(p->types, as_string(name_buf));
        if (existing) {
            if (new_alloc) {
                free(type);
                new_alloc = false;
            }
            type = existing;
        }
    }

    cached = ht_lookup(p->types, as_string(name_buf));
    if (cached) {
        if (new_alloc) {

            free(type);
            new_alloc=false;
        }
        free(name_buf.mem);
        type = cached;
    } else {
        ht_set(p->types, as_string(name_buf), type);
        group_claim(p->ag, type);
        group_claim(p->ag, name_buf.mem);
    }

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

#define expression(p) subexpr(p, XPREC_ASSIGN)
static UCLExpression *subexpr(USE Parser, enum expr_prec);

static UCLType *_Nonnull array_of(
    USE Parser p, size_t len, UCLType *_Nonnull mem_type
) {
    CharBuffer name = {};
    write_type(mem_type, &name);
    char numbuf[MAX_SIZE_DIGITS + 3];
    size_t sz = sprintf(numbuf, "[%zu]", len);
    append_chars(&name, sz, (uchar *)numbuf);
    String key = as_string(name);
    UCLType *type = ht_lookup(p->types, key);
    if (type) {
        free(name.mem);
    } else {
        type = checked_malloc(sizeof(UCLType));
        type->tag = TYPE_ARRAY;
        type->array.len = len;
        type->array.member_type = mem_type;
        ht_set(p->types, key, type);
        group_claim(p->ag, name.mem);
        group_claim(p->ag, type);
    }
    return type;
}

static UCLExpression *_Nullable array(USE Parser p) {
    size_t start = p->next_tok_i - 1;
    if (advance(p).type == TOKEN_R_SQUARE) {
        error_at(p, 0, "Empty arrays are not supported");
        return NULL;
    }

    size_t cap = 8;

    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_ARRAY;
    size_t alloc_index = p->build_allocs.count;
    tracked_alloc(
        p, sizeof(struct array_definition) + sizeof(UCLExpression *[8])
    );

#define PTR (p->build_allocs.ptrs[alloc_index])
#define ARR ((struct array_definition *)PTR)
    ARR->size = 0;
    ARR->type = NULL;
    do {
        if (ARR->size == cap) {
            cap = new_capacity(cap);
            PTR = checked_realloc(
                PTR,
                sizeof(struct array_definition) + sizeof(UCLExpression *) * cap
            );
        }
        if (!(ARR->vals[ARR->size++] = expression(p))) return NULL;
    } while (advance_on(p, TOKEN_COMMA) && (advance(p), true));

    if (!consume(p, TOKEN_R_SQUARE, "Expect ']' after array")) return NULL;

    expr->array = PTR;
    expr->type = NULL;
    expr->span.start = start;
    expr->span.end = p->next_tok_i;
    return expr;
#undef PTR
#undef ARR
}

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

#if CHAR_MIN < 0
    return (ucl_i8){.s = *(*chr)++};
#else
    return (ucl_i8){.u = *(*chr)++};
#endif
}

static UCLExpression *_Nonnull literal(USE Parser p) {
    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_LITERAL;
    expr->span.start = p->next_tok_i - 1;
    expr->span.end = p->next_tok_i;
    TokenType ty;
    switch (ty = current(p).type) {
        case TOKEN_CHAR: {
            expr->type = ht_lookup(p->types, (String){2, "i8"});
            const char *text = current(p).lexeme.text + 1;
            expr->literal.tag = LIT_I8;
            expr->literal._i8 = parse_char(&text);
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
                    expr->type = ht_lookup(p->types, (String){2, "i8"});
                    expr->literal._i8.u = value;
                    return expr;
                case 2:
                    expr->literal.tag = LIT_I16;
                    expr->type = ht_lookup(p->types, (String){3, "i16"});
                    expr->literal._i16.u = value;
                    return expr;
                case 0:
                case 3:
                    expr->literal.tag = LIT_I32;
                    expr->type = ht_lookup(p->types, (String){3, "i32"});
                    expr->literal._i32.u = value;
                    return expr;
                case 4:
                    expr->literal.tag = LIT_I64;
                    expr->type = ht_lookup(p->types, (String){3, "i64"});
                    expr->literal._i64.u = value;
                    return expr;
                default: UNREACHABLE("Modulo operation");
            }
        }
    }
}

static UCLExpression *_Nullable grouping(USE Parser p) {
    advance(p);
    size_t start = p->next_tok_i;
    UCLExpression *grouped = expression(p);
    if (!consume(
            p, TOKEN_R_PAREN, "Expected ')' after parenthesized expression"
        )) {
        return NULL;
    }
    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_GROUP;
    expr->grouped = grouped;
    expr->span.start = start;
    expr->span.end = p->next_tok_i;
    return expr;
}

static UCLExpression *_Nullable ident(USE Parser p) {
    Token tok = current(p);
    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_VAR;
    expr->var_name = intern(p->idents, tok.lexeme);
    expr->type = type_of_binding(p, &p->current_vars, expr->var_name);
    expr->span.start = p->next_tok_i - 1;
    expr->span.end = p->next_tok_i;
    return expr;
}

static UCLExpression *_Nullable unary(USE Parser p) {
    REQUIRE(p->next_tok_i > 0);
    size_t start = p->next_tok_i - 1;
    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_UNARY;
    expr->unary.op = current(p).type;
    advance(p);
    expr->unary.operand = subexpr(p, XPREC_UNARY);
    expr->span.start = start;
    expr->span.end = p->next_tok_i;
    return expr;
}

static UCLExpression *_Nullable typecast(USE Parser p) {
    size_t start = p->next_tok_i - 1;
    UCLType *type = read_type(p);
    if (type == NULL) return NULL;
    if (!consume(p, TOKEN_R_CURLY, "Expect '}' after cast type")) {
        return NULL;
    }

    UCLExpression *expr = alloc_expr(p);
    expr->tag = XPR_TYPECAST;
    expr->type = type;
    advance(p);
    expr->type_cast.inner = subexpr(p, XPREC_UNARY);
    expr->span.start = start;
    expr->span.end = p->next_tok_i;
    return expr;
}

static UCLExpression *_Nullable index_(
    USE Parser p, UCLExpression *_Nonnull e
) {
    advance(p);
    UCLExpression *new_expr = alloc_expr(p);
    new_expr->tag = XPR_INDEX;
    new_expr->index.array = e;
    new_expr->index.index = expression(p);
    new_expr->span.start = e->span.start;
    if (!consume(p, TOKEN_R_SQUARE, "expect ']' after array index.")) {
        return NULL;
    }
    new_expr->span.end = p->next_tok_i;
    e = new_expr;

    return e;
}

static UCLExpression *_Nullable call(USE Parser p, UCLExpression *_Nonnull e) {
    static thread_local UCLExpression *ARGS_BUF[256];
    UCLExpression *new_expr = alloc_expr(p);
    new_expr->tag = XPR_FUNC_CALL;
    new_expr->func_call = ALLOCATE(p, struct function_call);
    new_expr->func_call->callee = e;
    uint8_t nargs = 0;
    if (!advance_on(p, TOKEN_R_PAREN)) {
        do {
            if (nargs == UINT8_MAX)
                error_at(p, 0, "Too many arguments - maximum is 255.");
            ARGS_BUF[nargs++] = expression(p);
        } while (advance_on(p, TOKEN_COMMA));
        if (!consume(p, TOKEN_R_PAREN, "Expect ')' after function arguments."))
            return NULL;
    }
    new_expr->func_call->nargs = nargs;
    if (nargs) {
        new_expr->func_call->args = ALLOCATE(p, UCLExpression *[nargs]);
        memcpy(
            new_expr->func_call->args, ARGS_BUF, sizeof(UCLExpression *) * nargs
        );
    } else {
        new_expr->func_call->args = NULL;
    }

    new_expr->span.start = e->span.start;
    new_expr->span.end = p->next_tok_i;
    e = new_expr;

    return e;
}

[[gnu::const, gnu::artificial]] static inline ParseRule get_rule(TokenType op);

static UCLExpression *_Nullable binary(
    USE Parser p, UCLExpression *_Nonnull e
) {
    UCLExpression *new_expr = alloc_expr(p);
    TokenType op = current(p).type;
    enum expr_prec prec = get_rule(op).precedence;
    advance(p);
    UCLExpression *rhs = subexpr(p, prec);
    new_expr->binary.op = op;
    new_expr->tag = XPR_BINARY;

    new_expr->binary.lhs = e;
    new_expr->binary.rhs = rhs;
    new_expr->span.start = e->span.start;
    new_expr->span.end = rhs->span.end;
    return new_expr;
}

static UCLExpression *_Nullable assign(
    USE Parser p, UCLExpression *_Nonnull e
) {
    UCLExpression *new_expr = alloc_expr(p);

    new_expr->tag = XPR_ASSIGN;
    new_expr->assign.op = current(p).type;
    new_expr->assign.place = e;
    UCLExpression *_Nonnull value = new_expr->assign.value = expression(p);
    new_expr->span.start = e->span.start;
    new_expr->span.end = value->span.end;

    return new_expr;
}

// clang-format off

// Pratt parsing rule table
static const ParseRule XPR_RULES[TOKEN_EOF + 1] = {
    [TOKEN_L_SQUARE]            = {array,       index_,    XPREC_CALL},
    [TOKEN_L_PAREN]             = {grouping,    call,      XPREC_CALL},
    [TOKEN_PLUS]                = {NULL,        binary,    XPREC_TERM},
    [TOKEN_MINUS]               = {unary,       binary,    XPREC_TERM},
    [TOKEN_MUL]                 = {unary,       binary,    XPREC_FACTOR},
    [TOKEN_DIV]                 = {NULL,        binary,    XPREC_FACTOR},
    [TOKEN_MOD]                 = {NULL,        binary,    XPREC_FACTOR},
    [TOKEN_SIGNED_MUL]          = {NULL,        binary,    XPREC_FACTOR},
    [TOKEN_SIGNED_DIV]          = {NULL,        binary,    XPREC_FACTOR},
    [TOKEN_SIGNED_MOD]          = {NULL,        binary,    XPREC_FACTOR},
    [TOKEN_PLUS_ASSIGN]         = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_MINUS_ASSIGN]        = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_MUL_ASSIGN]          = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_DIV_ASSIGN]          = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_MOD_ASSIGN]          = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SIGNED_MUL_ASSIGN]   = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SIGNED_DIV_ASSIGN]   = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SIGNED_MOD_ASSIGN]   = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_EQ]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_NE]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_LT]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_GT]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_LE]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_GE]                  = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_SIGNED_LT]           = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_SIGNED_GT]           = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_SIGNED_LE]           = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_SIGNED_GE]           = {NULL,        binary,    XPREC_ORDER},
    [TOKEN_DEREF]               = {unary,       NULL,      XPREC_NONE},
    [TOKEN_REF]                 = {unary,       NULL,      XPREC_NONE},
    [TOKEN_LOGICAL_NOT]         = {unary,       NULL,      XPREC_NONE},
    [TOKEN_LOGICAL_AND]         = {NULL,        binary,    XPREC_LOGIC},
    [TOKEN_LOGICAL_OR]          = {NULL,        binary,    XPREC_LOGIC},
    [TOKEN_BIT_NOT]             = {NULL,        binary,    XPREC_BMASK},
    [TOKEN_BIT_AND]             = {unary,       binary,    XPREC_BMASK},
    [TOKEN_BIT_OR]              = {NULL,        binary,    XPREC_BMASK},
    [TOKEN_BIT_XOR]             = {NULL,        binary,    XPREC_BMASK},
    [TOKEN_BIT_AND_ASSIGN]      = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_BIT_OR_ASSIGN]       = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_BIT_XOR_ASSIGN]      = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SHR_LOG]             = {NULL,        binary,    XPREC_BSHIFT},
    [TOKEN_SHR_ARITH]           = {NULL,        binary,    XPREC_BSHIFT},
    [TOKEN_SHL]                 = {NULL,        binary,    XPREC_BSHIFT},
    [TOKEN_SHR_LOG_ASSIGN]      = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SHR_ARITH_ASSIGN]    = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_SHL_ASSIGN]          = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_ASSIGN]              = {NULL,        assign,    XPREC_ASSIGN},
    [TOKEN_UNSIGNED_BITCAST_8]  = {unary,       NULL,      XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_16] = {unary,       NULL,      XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_32] = {unary,       NULL,      XPREC_NONE},
    [TOKEN_UNSIGNED_BITCAST_64] = {unary,       NULL,      XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_8]    = {unary,       NULL,      XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_16]   = {unary,       NULL,      XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_32]   = {unary,       NULL,      XPREC_NONE},
    [TOKEN_SIGNED_BITCAST_64]   = {unary,       NULL,      XPREC_NONE},
    [TOKEN_TYPE_CAST]           = {typecast,    NULL,      XPREC_NONE},
    [TOKEN_OPAQUE_8]            = {literal,     NULL,      XPREC_NONE},
    [TOKEN_OPAQUE_16]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_OPAQUE_32]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_OPAQUE_64]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_WHILE]               = {literal,     NULL,      XPREC_NONE},
    [TOKEN_STR]                 = {literal,     NULL,      XPREC_NONE},
    [TOKEN_CHAR]                = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_DEC]             = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_DEC_8]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_DEC_16]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_DEC_32]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_DEC_64]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_BIN]             = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_BIN_8]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_BIN_16]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_BIN_32]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_BIN_64]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_OCT]             = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_OCT_8]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_OCT_16]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_OCT_32]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_OCT_64]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_HEX]             = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_HEX_8]           = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_HEX_16]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_HEX_32]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_INT_HEX_64]          = {literal,     NULL,      XPREC_NONE},
    [TOKEN_IDENT]               = {ident,       NULL,      XPREC_NONE},
};
// clang-format on

[[gnu::const, gnu::artificial]] static inline ParseRule get_rule(TokenType op) {
    return XPR_RULES[op];
}

// parse a subexpression using the XPR_RULES Pratt parsing table
static UCLExpression *subexpr(USE Parser p, enum expr_prec prec) {
    PrefixFn pref;
    if (!(pref = XPR_RULES[current(p).type].prefix)) {
        error_at(p, 0, "Expect expression (t: %d).", (int)current(p).type);
        return NULL;
    }

    UCLExpression *a = pref(p);

    while (prec <= XPR_RULES[peek(p).type].precedence) {
        InfixFn infix = XPR_RULES[advance(p).type].infix;
        a = infix(p, a);
    }

    // typeck_expr((TypeckCtx){.ag = p->ag, .types = p->types}, a);

    return a;
}

[[gnu::cold]]
static int ident_printf_len(const String *s) {
    if (s->len > 80) [[clang::unlikely]] { return 80; }
    return s->len;
}

static Statement var_decl(USE Parser p, Placement plcmnt) {
    size_t start = p->next_tok_i;
    Statement stmt = {.tag = STMT_VAR_DEC};
    stmt.span.start = start;
    if (!consume(p, TOKEN_IDENT, "Expect variable name")) {
        stmt.tag = STMT_UNPARSABLE;
        goto end;
    }
    stmt.declaration.ident = intern(p->idents, current(p).lexeme);
    stmt.declaration.placement = plcmnt;
    bool type_known = false;
    UCLType *type = NULL;
    if (advance_on(p, TOKEN_COLON)) {
        type = read_type(p);
        if (type == NULL) {
            stmt.tag=STMT_UNPARSABLE;
            goto end;
        }
        type_known = true;
    }

    if (advance_on(p, TOKEN_ASSIGN)) {
        advance(p);
        stmt.declaration.value = expression(p);
        if (stmt.declaration.value == NULL) {
            stmt.tag=STMT_UNPARSABLE;
            goto end;
        }
        if (type_known) {
            if (stmt.declaration.value->type != type) {
                CharBuffer buf = {};
                write_type(type, &buf);
                append_chars(&buf, 31, u8" with an initial value of type ");
                write_type(stmt.declaration.value->type, &buf);
                append_chars(&buf, 1, u8"");
                error_at(p, 0, "Type mismatch: declared a %s", buf.mem);
                free(buf.mem);
            }
        } else if ((type = stmt.declaration.value->type) != NULL) {
            type_known = true;
        }
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

    if (!consume(p, TOKEN_SEMICOLON, "Expect ';' after declaration")) {
        stmt.tag = STMT_UNPARSABLE;
    }
end:
    stmt.span.end = p->next_tok_i;
    return stmt;
}

static Statement block_stmt(USE Parser p) {
    size_t cap = 8;
    size_t count = 0;
    size_t alloc_index = p->build_allocs.count;
    size_t start = p->next_tok_i;
    tracked_alloc(p, sizeof(Statement[8]));
#define BODY (p->build_allocs.ptrs[alloc_index])

    TokenType ty;

    while ((ty = peek(p).type), ty != TOKEN_R_CURLY && ty != TOKEN_EOF) {
        if (cap <= count) {
            BODY = checked_reallocarray(
                BODY, new_capacity(cap), sizeof(Statement)
            );
        }

        ((Statement *)BODY)[count++] = get_statement(p);
    }

    if (!advance_on(p, TOKEN_R_CURLY)) {
        error_at(
            p, 0, "Unclosed block (started on line %zu)", p->tokens[start].line
        );
    }
    struct block *block = ALLOCATE(p, struct block);
    block->size = count;
    block->statements = BODY;
    Statement stmt = {
        .tag = STMT_BLOCK,
        .block = block,
        .span.start = start,
        .span.end = p->next_tok_i
    };
    return stmt;
#undef BODY
}

static Statement cond_stmt(USE Parser p) {
    Statement stmt = {
        .tag = STMT_IF, .conditional = {}, .span.start = p->next_tok_i
    };

    if (!consume(p, TOKEN_L_PAREN, "Expect '(' after 'if'")) {
        stmt.tag = STMT_UNPARSABLE;
        goto end;
    }

    UCLExpression *cond = expression(p);

    if (!cond || !consume(p, TOKEN_R_PAREN, "Expect ')' after condition")) {
        stmt.tag = STMT_UNPARSABLE;
        goto end;
    }

    stmt.conditional.cond = cond;

    Statement *if_body = ALLOCATE(p, Statement);
    *if_body = get_statement(p);

    if (if_body->tag == STMT_UNPARSABLE) {
        stmt.tag = STMT_UNPARSABLE;
        goto end;
    }

    stmt.conditional.if_body = if_body;

    if (advance_on(p, TOKEN_ELSE)) {
        Statement *else_body = NULL;
        else_body = ALLOCATE(p, Statement);
        *else_body = get_statement(p);

        if (else_body->tag == STMT_UNPARSABLE) {
            stmt.tag = STMT_UNPARSABLE;
            goto end;
        }

        stmt.conditional.else_body = else_body;
    }

end:
    stmt.span.end = p->next_tok_i;
    return stmt;
}

static Statement expr_stmt(USE Parser p) {
    UCLExpression *_Nonnull expr = expression(p);
    Statement stmt = {.tag = STMT_EXPR};
    if (expr) {
        stmt = (Statement){
            .tag = STMT_EXPR,
            .expr = expr,
            .span.start = expr->span.start,
        };
    } else {
        stmt = (Statement){.tag = STMT_UNPARSABLE};
    }
    if (!consume(p, TOKEN_SEMICOLON, "Expect ';' after expression statement")) {
        stmt.tag = STMT_UNPARSABLE;
    }
    stmt.span.end = p->next_tok_i;
    return stmt;
}

static Statement while_stmt(USE Parser p) { TODO(p); }

static Statement foreach_stmt(USE Parser p) { TODO(p); }

[[gnu::cold]]
static void resync(USE Parser p) {
    while (current(p).type != TOKEN_EOF) {
        switch (current(p).type) {
            case TOKEN_SEMICOLON: advance(p); return;
            case TOKEN_FOREACH:
            case TOKEN_FUNC:
            case TOKEN_IF:
            case TOKEN_LET:
            case TOKEN_STATIC:
            case TOKEN_WHILE:
            case TOKEN_RETURN: return;
            case TOKEN_EOF: UNREACHABLE("explicit check");
            default: advance(p);
        }
    }
    p->panicking = false;
}

Statement get_statement(USE Parser p) {
    size_t ba_ct = p->build_allocs.count;
    Statement s = (Statement){.tag = STMT_UNPARSABLE};

    switch (advance(p).type) {
        case TOKEN_EOF: return (Statement){.tag = STMT_END};
        case TOKEN_LET: s = var_decl(p, STORE_STACK); break;
        case TOKEN_STATIC: s = var_decl(p, STORE_STATIC); break;
        case TOKEN_IF: s = cond_stmt(p); break;
        case TOKEN_L_CURLY: s = block_stmt(p); break;
        case TOKEN_WHILE: s = while_stmt(p); break;
        case TOKEN_FOREACH: s = foreach_stmt(p); break;
        default: s = expr_stmt(p); break;
    }

    if (p->panicking) {
        s.tag = STMT_UNPARSABLE;
        while (p->build_allocs.count > ba_ct) {
            free(p->build_allocs.ptrs[--p->build_allocs.count]);
        }
        resync(p);
    } else {
        assert(s.tag != STMT_UNPARSABLE);
        while (p->build_allocs.count > ba_ct) {
            group_claim(p->ag, p->build_allocs.ptrs[--p->build_allocs.count]);
        }
        // typeck_stmt((TypeckCtx){.ag = p->ag, .types = p->types}, &s);
    }

    return s;
}

void free_parser([[clang::release_handle("parser")]] Parser p) {
    for (size_t i = 0; i < p->build_allocs.count; i++) {
        free(p->build_allocs.ptrs[i]);
    }

    struct var_scope *scope = &p->current_vars;
    while (scope) {
        ht_cleanup(scope->vars, NULL);
        scope = scope->outer;
    }
    free_scanner(p->scanner);
    free_pool(p->idents);
    ht_cleanup(p->types, NULL);
    free(p->build_allocs.ptrs);

    group_free(p->ag);
    free(p->tokens);

    free(p);
}

static void write_indent(CharBuffer *buf, size_t indent) {
    while (indent > 8) [[clang::unlikely]] {
        append_chars(buf, 16, u8"                ");
        indent -= 8;
    }
    if (indent) append_chars(buf, indent * 2, u8"                ");
}

static void dbg_expr_inner(
    USE const Parser p,
    const UCLExpression *_Nonnull expr,
    CharBuffer *buf,
    size_t indent
) {
    REQUIRE(expr->span.end > expr->span.start);
    REQUIRE(expr->span.end <= p->ntokens);

#define FIXED(s) append_chars(buf, sizeof(s) - 1, u8##s)
    const Token end_t = p->tokens[expr->span.end - 1];

    const char *start = p->tokens[expr->span.start].lexeme.text;
    const char NODEREF *end = &end_t.lexeme.text[end_t.lexeme.len];
    write_indent(buf, indent);
    FIXED("<\"");
    append_chars(buf, end - start, (const uchar *)start);
    FIXED("\">:\n");
    write_indent(buf, indent);
    FIXED(" | ");

    switch (expr->tag) {
        case XPR_LITERAL:
            FIXED("Literal ");
            write_type(expr->type, buf);
            FIXED(" `");
            append_chars(buf, end - start, (const uchar *)start);
            FIXED("`");
            break;
        case XPR_VAR:
            FIXED("Variable `");
            append_chars(buf, end - start, (const uchar *)start);
            FIXED("`");
            break;
        case XPR_UNARY: {
            FIXED("Unary `");
            String s = p->tokens[expr->span.start].lexeme;
            append_chars(buf, s.len, (const uchar *)s.text);
            FIXED("`(\n");
            dbg_expr_inner(p, expr->unary.operand, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
        case XPR_BINARY: {
            FIXED("Binary `");
            String s = p->tokens[expr->binary.rhs->span.start - 1].lexeme;
            append_chars(buf, s.len, (const uchar *)s.text);
            FIXED("`(\n");
            dbg_expr_inner(p, expr->binary.lhs, buf, indent + 1);
            FIXED(",\n");
            dbg_expr_inner(p, expr->binary.rhs, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
        case XPR_FUNC_CALL: TODO();
        case XPR_GROUP: {
            FIXED("Group (\n");
            dbg_expr_inner(p, expr->grouped, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
        case XPR_TYPECAST: {
            FIXED("Cast to ");
            write_type(expr->type, buf);
            FIXED("(\n");
            dbg_expr_inner(p, expr->type_cast.inner, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
        case XPR_ARRAY: {
            FIXED("Array [\n");
            for (size_t i = 0; i < expr->array->size; i++) {
                dbg_expr_inner(p, expr->array->vals[i], buf, indent + 1);
                FIXED(",\n");
            }

            write_indent(buf, indent);
            FIXED("]");
            break;
        }
        case XPR_ASSIGN: {
            if (expr->assign.op == TOKEN_EQ) {
                FIXED("Assign(\n");
            } else {
                FIXED("Compound Assign <");
                String s = p->tokens[expr->assign.value->span.start - 1].lexeme;
                append_chars(buf, s.len, (const uchar *)s.text);
                FIXED(">(\n");
            }
            dbg_expr_inner(p, expr->assign.place, buf, indent + 1);
            FIXED(",\n");
            dbg_expr_inner(p, expr->assign.value, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
        case XPR_INDEX: {
            FIXED("Index(\n");
            dbg_expr_inner(p, expr->index.array, buf, indent + 1);
            FIXED(",\n");
            dbg_expr_inner(p, expr->index.index, buf, indent + 1);
            FIXED("\n");
            write_indent(buf, indent);
            FIXED(")");
            break;
        }
    }
}

void dbg_expr(USE const Parser p, const UCLExpression *_Nonnull expr) {
    CharBuffer buf = {};
    dbg_expr_inner(p, expr, &buf, 0);

    size_t len = buf.size;
    uchar *c = buf.mem;

    NODEREF uchar *end = buf.mem + buf.size;

    while (c < end) {
        auto w = write(STDERR_FILENO, c, len);
        if (w < 0) {
            perror("Failed to write to stderr");
            free(buf.mem);
            return;
        }
        c += w;
        len -= w;
    }

    free(buf.mem);
}

void dbg_stmt_inner(
    USE const Parser p, const Statement *stmt, CharBuffer *buf, size_t indent
) {
    if (stmt->tag == STMT_END) return;
    if (stmt->tag == STMT_UNPARSABLE) return;

    REQUIRE(stmt->span.end > stmt->span.start);
    REQUIRE(stmt->span.end <= p->ntokens);

    const Token end_t = p->tokens[stmt->span.end - 1];

    const char *start = p->tokens[stmt->span.start].lexeme.text;
    const char *end = &end_t.lexeme.text[end_t.lexeme.len];

    write_indent(buf, indent);
    FIXED("Statement <\"");
    append_chars(buf, end - start, (const uchar *)start);
    FIXED("\">:\n");
    write_indent(buf, indent);

    switch (stmt->tag) {
        case STMT_UNPARSABLE: FIXED("UNPARSABLE\n"); break;
        case STMT_BLOCK: {
            FIXED("Block: {\n");
            auto block = stmt->block;
            for (size_t i = 0; i < block->size; i++) {
                dbg_stmt_inner(p, &block->statements[i], buf, indent + 1);
            }
            write_indent(buf, indent);
            FIXED("}");
        }
        case STMT_IF: TODO();
        case STMT_WHILE: TODO();
        case STMT_FOREACH: TODO();
        case STMT_RETURN: TODO();
        case STMT_EXPR: {
            FIXED("Expr:\n");
            dbg_expr_inner(p, stmt->expr, buf, indent + 1);
            break;
        }
        case STMT_BREAK: TODO();
        case STMT_VAR_DEC: {
            auto dec = &stmt->declaration;
            FIXED("Declare ");
            switch (dec->placement) {
                case STORE_STACK: break;
                case STORE_STATIC: FIXED("[static] ");
            }

            append_chars(buf, dec->ident->len, (const uchar *)dec->ident->text);

            if (dec->type) {
                FIXED(": [");
                write_type(dec->type, buf);
                FIXED("]");
            }

            if (dec->value) {
                FIXED(":\n");
                dbg_expr_inner(p, dec->value, buf, indent + 1);
            }

            break;
        }
        case STMT_FUN_DEC: TODO();
        case STMT_EXTERN_VAR: TODO();
        case STMT_EXTERN_FUN: TODO();
        case STMT_SYSCALL: TODO();
        case STMT_END: UNREACHABLE("explicitly handled already");
    }
}

#undef FIXED

void dbg_stmt(USE const Parser p, const Statement *_Nonnull stmt) {
    CharBuffer buf = {};
    dbg_stmt_inner(p, stmt, &buf, 0);

    size_t len = buf.size;
    uchar *c = buf.mem;

    NODEREF uchar *end = buf.mem + buf.size;

    while (c < end) {
        auto w = write(STDERR_FILENO, c, len);
        if (w < 0) {
            perror("Failed to write to stderr");
            free(buf.mem);
            return;
        }
        c += w;
        len -= w;
    }

    free(buf.mem);
    write(STDERR_FILENO, "\n", 1);
}
