/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_AST
#define UCLP_AST
#include <stdint.h>

#include "include/token.h"
#include "lang/types.h"
#include "sized_string.h"
#include "tagged_union.h"

typedef enum : uint8_t { STORE_STACK, STORE_STATIC } Placement;

typedef struct { size_t start; size_t end; } AstSpan;

typedef struct expression {
    TU_ENUM(
        tag,
        uint8_t,
        XPR_LITERAL,
        XPR_VAR,
        XPR_UNARY,
        XPR_BINARY,
        XPR_FUNC_CALL,
        XPR_GROUP,
        XPR_TYPECAST,
        XPR_ARRAY,
        XPR_ASSIGN,
        XPR_INDEX,
    );

    union {
        struct unary_expression {
            TokenType op;
            struct expression *_Nonnull operand;
        } unary;

        struct assignment {
            TokenType op;
            struct expression *_Nonnull place;
            struct expression *_Nonnull value;
        } assign;

        struct binary_expression {
            TokenType op;
            struct expression *_Nonnull lhs;
            struct expression *_Nonnull rhs;
        } binary;

        struct type_cast {
            struct expression *_Nonnull inner;
        } type_cast;

        struct function_call *_Nonnull func_call;
        struct array_definition *_Nonnull array;

        String *_Nonnull var_name;

        struct index_expression {
            struct expression *_Nonnull array;
            struct expression *_Nonnull index;
        } index;
        struct expression *_Nonnull grouped;
        struct literal_expression {
            TU_ENUM(tag, uint8_t, LIT_STR, LIT_I8, LIT_I16, LIT_I32, LIT_I64);

            union {
                String string;
                ucl_i8 _i8;
                ucl_i16 _i16;
                ucl_i32 _i32;
                ucl_i64 _i64;
            };
        } literal;

    };

    UCLType *_Nullable type;
    AstSpan span;
} UCLExpression;

struct array_definition {
    UCLType *_Nonnull type;
    size_t size;
    UCLExpression *vals [[clang::counted_by(size)]][];
};

// a single program statement - can be one of the following:
//
// * A variable declaration (with or without initial value)
// * An assignment
// * A block of expressions which run within their own scope.
// * A conditional or series of conditionals
typedef struct statement {
    TU_ENUM(
        tag,
        uint8_t,
        STMT_UNPARSABLE,
        STMT_BLOCK,
        STMT_IF,
        STMT_WHILE,
        STMT_FOREACH,
        STMT_RETURN,
        STMT_EXPR,
        STMT_BREAK,
        STMT_VAR_DEC,
        STMT_FUN_DEC,
        STMT_EXTERN_VAR,
        STMT_EXTERN_FUN,
        STMT_SYSCALL,
        STMT_END,
    );

    union {
        struct {
            String *_Nonnull ident;
            struct expression *_Nullable value;
            Placement placement;
        } declaration;

        struct block *_Nonnull block;

        struct {
            struct expression *_Nonnull cond;
            struct statement *_Nonnull if_body;
            struct statement *_Nullable else_body;
        } conditional;

        struct {
            struct expression *_Nonnull cond;
            struct block *_Nonnull body;
        } while_loop;

        struct {
            String iter_var;
            struct array *_Nonnull array;
            struct block *_Nonnull body;
        } foreach_loop;

        struct expression *_Nonnull expr;
        struct function_decl *_Nonnull fun_dec;
    };
    AstSpan span;
} Statement;

// A sequence of expressions within their own scope;
struct block {
    size_t size;
    Statement *_Nonnull statements [[clang::counted_by(size)]];
};

struct function_call {
    struct expression *_Nonnull callee;
    uint8_t nargs;
    struct expression *_Nonnull args [[clang::counted_by(nargs)]];
};

struct function_arg {
    String name;
    UCLType *_Nonnull type;
};

// A function declaration
struct function_decl {
    TU_ENUM(tag, uint8_t, FUNC_DEF, FUNC_INTERNAL, FUNC_DYNAMIC);
    String name;
    uint8_t nargs;
    union {
        struct block body;
        struct {
            String *_Nonnull symbol;
            String *_Nonnull dlopen_name;
        } dynamic;
    };
    struct function_arg *_Nonnull args [[clang::counted_by(nargs)]];
};

#endif /* UCLP_AST */
