/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_AST
#define UCLP_AST
#include <stdint.h>

#include "lang/types.h"
#include "sized_string.h"

// an enum (backed by uint8_t) discriminant, which, through clang attributes,
// can be reliably used in tagged unions, with Clang static analysis enforcing
// proper initialization
#define TU_ENUM(tag, ...) \
    enum [[clang::enum_extensibility(closed)]] : uint8_t { \
        __VA_ARGS__ \
    } tag [[clang::require_explicit_initialization]]

// the destination of an assignment
struct place {
    TU_ENUM(tag, PLACE_VAR, PLACE_PTR, PLACE_ARR_MEM);

    UCLType *_Nonnull type;

    union {
        String var_ident;
        struct expression *_Nonnull ptr;

        struct array_member {
            struct array *_Nonnull array;
            size_t offset;
        } arr_mem;
    };
};

struct array {
    UCLType type;
    size_t size;
};
typedef enum : uint8_t { STORE_STACK, STORE_STATIC } Placement;

struct conditional {
    struct expression *_Nonnull cond;
    struct block *_Nonnull if_body;

    struct elif_block {
        struct expression *_Nonnull cond;
        struct block *_Nonnull body;
        struct elif_block *_Nullable next;
    } *_Nullable elif_block;

    struct block *_Nullable else_body;
};

// a single program statement - can be one of the following:
//
// * A variable declaration (with or without initial value)
// * An assignment
// * A block of expressions which run within their own scope.
// * A conditional or series of conditionals
struct statement {
    TU_ENUM(
        tag,
        STMT_DECL,
        STMT_ASSIGN,
        STMT_BLOCK,
        STMT_COND,
        STMT_WHILE,
        STMT_FOREACH,
        STMT_FUNC_CALL
    );

    union {
        struct {
            String *_Nonnull ident;
            struct expression *_Nullable value;

            Placement placement;
        } declaration;

        struct {
            struct place *_Nonnull assign_dest;
            struct expression *_Nonnull assign_expr;
        };

        struct block *_Nonnull block;

        struct conditional *_Nonnull conditional;

        struct {
            struct expression *_Nonnull cond;
            struct block *_Nonnull body;
        } while_loop;

        struct {
            String iter_var;
            struct array *_Nonnull array;
            struct block *_Nonnull body;
        } foreach_loop;

        struct function_call *_Nonnull function_call;
    };
};

// A sequence of expressions within their own scope;
struct block {
    size_t body_size;
    struct statement *_Nonnull statements [[clang::counted_by(body_size)]];
    UCLType *_Nullable return_type;
};

struct expression {
    TU_ENUM(
        tag,
        // identifiers starting with "E" followed by a number or another
        // upprecase letter are reserved for system errors
        XPR_LITERAL,
        XPR_VAR,
        XPR_UNARY,
        XPR_BINARY,
        XPR_FUNC_CALL,
    );

    union {
        struct unary_expression {
            TU_ENUM(
                tag,
                UNARY_LOGNOT,
                UNARY_BITNOT,
                UNARY_NEG,
                UNARY_CAST_S,
                UNARY_CAST_U,
                UNARY_ADDROF,
                UNARY_DEREF
            );
            struct expression *_Nonnull operand;
        } unary;

        struct binary_expression {
            TU_ENUM(
                tag,
                BINARY_MUL_U,
                BINARY_DIV_U,
                BINARY_MOD_U,
                BINARY_MUL_S,
                BINARY_DIV_S,
                BINARY_MOD_S,
                BINARY_ADD,
                BINARY_SUB,
                BINARY_BITAND,
                BINARY_BITOR,
                BINARY_BITXOR,
                BINARY_SHIFTR_U,
                BINARY_SHIFTR_S,
                BINARY_SHIFTL,
                BINARY_CMP_EQ,
                BINARY_CMP_NE,
                BINARY_CMP_LT_U,
                BINARY_CMP_GT_U,
                BINARY_CMP_LE_U,
                BINARY_CMP_GE_U,
                BINARY_CMP_LT_S,
                BINARY_CMP_GT_S,
                BINARY_CMP_LE_S,
                BINARY_CMP_GE_S,
                BINARY_LOGAND,
                BINARY_LOGOR,
            );
            struct expression *_Nonnull operands[2];
        } binary;

        String var_name;
        void *_Nonnull literal;
    };

    UCLType *_Nonnull type;
};
struct function_call {
    String name;
    uint8_t nargs;
    struct expression *_Nonnull args [[clang::counted_by(nargs)]];
};

struct function_arg {
    String name;
    UCLType *_Nonnull type;
};

// A function declaration
struct function_decl {
    TU_ENUM(tag, FUNC_INTERNAL, FUNC_DYNAMIC);
    String name;
    uint8_t nargs;

    union {
        struct block internal;

        struct {
            String *_Nonnull symbol;
            String *_Nonnull dlopen_name;
        } dynamic;
    };
    struct function_arg *_Nonnull args [[clang::counted_by(nargs)]];
};

struct top_level_statement {
    union {
        struct function_decl function_decl;
        struct statement statement;
    };

    enum : bool {
        TOP_LVL_FUNC_DECL,
        TOP_LVL_STATEMENT,
    } tag [[clang::require_explicit_initialization]];
};

typedef struct ast {
    size_t len;
    struct top_level_statement *_Nonnull root [[clang::counted_by(len)]];
} Ast;

#undef TU_ENUM

#include "alloc.h"
#include "token.h"

typedef enum [[clang::enum_extensibility(open)]] {
    AST_BUILD_SUCCESS,
    AST_BUILD_UNEXPECTED_EOF,
    AST_BUILD_UNIMPLEMENTED,
} AstBuildErr;

// either the root of the Abstract Syntax Tree, or the first token that failed
// to be parsed
union build_ast_result {
    Ast ast;

    Token err_token;
};

AstBuildErr build_ast(
    struct token_sequence seq,
    AllocGroup ag,
    union build_ast_result *_Nonnull result
);

#endif /* UCLP_AST */
