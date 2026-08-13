/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
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

// the destination of an assignment
struct place {
    TU_ENUM(tag, uint8_t, PLACE_VAR, PLACE_PTR, PLACE_ARR_MEM);

    UCLType *_Nonnull type;

    union {
        String var_ident;
        struct expression *_Nonnull ptr;

        struct array_member {
            struct expression *_Nonnull array;
            size_t offset;
        } arr_mem;
    };
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

typedef struct expression {
    TU_ENUM(
        tag,
        uint8_t,
        // identifiers starting with "E" followed by a number or another
        // upprecase letter are reserved for system errors
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

        struct binary_expression {
            TokenType op;
            struct expression *_Nonnull operands[2];
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

    UCLType *_Nonnull type;
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
        STMT_UNPARSABLE,
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

        struct expression *_Nonnull expr;
    };
} Statement;

// A sequence of expressions within their own scope;
struct block {
    size_t body_size;
    Statement *_Nonnull statements [[clang::counted_by(body_size)]];
    UCLType *_Nullable return_type;
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
    TU_ENUM(tag, uint8_t, FUNC_INTERNAL, FUNC_DYNAMIC);
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

#include "mem/arena.h"
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
