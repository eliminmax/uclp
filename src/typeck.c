/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "ast.h"
#include "colorize.h"
#include "lang/types.h"
#include "mem/checked.h"
#include "mem/dynarray.h"
#include "safety.h"
#include "table.h"
#include "todo.h"
#include "typeck.h"

// Approximation of digits in SIZE_MAX, adapted from chux's StackOverflow answer
// (see https://stackoverflow.com/a/43788824)
constexpr size_t MAX_SIZE_DIGITS = ((sizeof(size_t) * CHAR_BIT) * 28 / 93 + 2);

Alignment ucl_alignof(const UCLType *_Nonnull type) {
    switch (type->tag) {
        case TYPE_OPAQUE: return type->opaque.alignment;
        case TYPE_I8: return ALIGN_8;
        case TYPE_I16: return ALIGN_16;
        case TYPE_I32: return ALIGN_32;
        case TYPE_I64: return ALIGN_64;
        case TYPE_FUNCTION: return ALIGN_PTR;
        case TYPE_POINTER: return ALIGN_PTR;
        case TYPE_ARRAY: return ucl_alignof(type->array.member_type);
        case TYPE_UNINHABITED: return 0;
    }
}

size_t ucl_sizeof(UCLType *_Nonnull type) {
    switch (type->tag) {
        case TYPE_I8: return 1;
        case TYPE_I16: return 2;
        case TYPE_I32: return 4;
        case TYPE_I64: return 8;
        case TYPE_OPAQUE: return type->opaque.size;
        case TYPE_FUNCTION: return 8;
        case TYPE_POINTER: return 8;
        case TYPE_ARRAY:
            return ucl_sizeof(type->array.member_type) * type->array.len;
        case TYPE_UNINHABITED: return 0;
    }
}

static void write_type_err(const UCLType *_Nonnull t);

void write_type(const UCLType *type, CharBuffer *buf) {
    switch (type->tag) {
        case TYPE_POINTER:
            if (type->pointee_type) {
                append_chars(buf, 1, u8"*");
                write_type(type->pointee_type, buf);
            } else {
                append_chars(buf, 8, u8"*dynamic");
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
            break;
        }
        case TYPE_FUNCTION: {
            append_chars(buf, 5, u8"func(");
            if (type->func->nargs) {
                write_type(type->func->arg_types[0], buf);
                for (int i = 1; i < type->func->nargs; i++) {
                    append_chars(buf, 1, u8",");
                    write_type(type->func->arg_types[i], buf);
                }
            }
            append_chars(buf, 1, u8")");
            if (type->func->return_type) {
                append_chars(buf, 2, u8"->");
                write_type(type->func->return_type, buf);
            }
            break;
        }
        case TYPE_UNINHABITED: append_chars(buf, 13, u8"<uninhabited>");
    }
}

UCLType *_Nonnull pointer_to(TypeckCtx ctx, UCLType *_Nonnull type) {
    CharBuffer b = {};
    append_chars(&b, 1, u8"*");
    write_type(type, &b);
    UCLType *_Nullable t;
    if ((t = ht_lookup(ctx.types, as_string(b)))) {
    } else {
        t = checked_malloc(sizeof(UCLType));
        t->tag = TYPE_POINTER;
        t->pointee_type = type;
        ht_set(ctx.types, as_string(b), t);
        group_claim(ctx.ag, t);
    }
    free(b.mem);
    return t;
}

[[gnu::cold]]
static void write_type_err(const UCLType *_Nonnull t) {
    static const char *TYPE_HL, *HL_END;
    // green; underlined
    if (!TYPE_HL) TYPE_HL = select_color(stderr, "\x1b[32;4m");
    if (!HL_END) HL_END = select_color(stderr, "\x1b[m");

    CharBuffer buf = (CharBuffer){};
    write_type(t, &buf);
    if (*TYPE_HL) fputs(TYPE_HL, stderr);
    fwrite(buf.mem, 1, buf.size, stderr);
    if (*HL_END) fputs(HL_END, stderr);
}

extern inline bool is_numeric(const UCLType *_Nonnull);

static bool ensure_numeric(
    const char *op, const UCLType *_Nonnull t, size_t line
) {
    if (is_numeric(t)) [[clang::likely]] { return true; }
    fprintf(
        stderr,
        "Error [line %zu]: %s requires a numeric operand, but got ",
        line,
        op
    );
    write_type_err(t);
    fputs(".\n", stderr);
    return false;
}

static bool typeck_unary(TypeckCtx ctx, UCLExpression *_Nonnull expr) {
    REQUIRE(expr->tag == XPR_UNARY);
    struct unary_expression *unary = &expr->unary;
    typeck_expr(ctx, expr->unary.operand);
    REQUIRE(unary->operand->type != nullptr);
    UCLType *_Nonnull inner_type = unary->operand->type;
    switch (unary->op) {
        case TOKEN_REF:
            fputs(
                "Warning: '.&' syntax is deprecated - use plain '&'.\n", stderr
            );
            [[fallthrough]];
        case TOKEN_BIT_AND: return expr->type = pointer_to(ctx, inner_type);

        case TOKEN_LOGICAL_NOT:
            if (!is_numeric(inner_type) && inner_type->tag != TYPE_POINTER) {
                fputs("Error: logical can't be perfomed on ", stderr);
                write_type_err(inner_type);
                fputs(
                    ". (Supported types are integral types and pointers).\n",
                    stderr
                );
                return false;
            }

        case TOKEN_BIT_NOT:
            if (!ensure_numeric("bitwise not", expr->type, expr->line))
                return false;
            expr->type = inner_type;
            return true;

        case TOKEN_MINUS:
            if (!ensure_numeric("unary negation", inner_type, expr->line))
                return false;
            expr->type = inner_type;
            return true;

        case TOKEN_SIGNED_BITCAST_8:
        case TOKEN_UNSIGNED_BITCAST_8:
            if (!ensure_numeric("bitcast", inner_type, expr->line)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){2, "i8"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_16:
        case TOKEN_UNSIGNED_BITCAST_16:
            if (!ensure_numeric("bitcast", inner_type, expr->line)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){3, "i16"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_32:
        case TOKEN_UNSIGNED_BITCAST_32:
            if (!ensure_numeric("bitcast", inner_type, expr->line)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){3, "i32"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_64:
        case TOKEN_UNSIGNED_BITCAST_64:
            if (!ensure_numeric("bitcast", inner_type, expr->line)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){3, "i64"});
            }
            return true;

        case TOKEN_DEREF:
            fputs(
                "Warning: '.*' syntax is deprecated - use plain '*'.\n", stderr
            );
            [[fallthrough]];
        case TOKEN_MUL: {
            if (inner_type->tag == TYPE_POINTER) [[clang::likely]] {
                if (inner_type->pointee_type != NULL) [[clang::likely]] {
                    expr->type = inner_type->pointee_type;
                    return true;
                } else {
                    fputs(
                        "Error: can't dereference dynamic pointer.\n", stderr
                    );
                    return false;
                }
            } else {
                fputs("Error: can't dereference non-pointer type ", stderr);
                write_type_err(inner_type);
                fputs(".\n", stderr);
                return false;
            }
        }

        default:
            UNREACHABLE("Parser would not have generated AST with invalid op");
    }
}

static bool bin_same_type(
    const char *op, size_t line, const UCLType *_Nonnull types[2]
) {
    if (types[0] == types[1]) [[clang::likely]]
        return true;

    fprintf(
        stderr,
        "Error [line %zu]: operator '%s' requires LHS and RHS to have the same "
        "type, but LHS is ",
        line,
        op
    );
    write_type_err(types[0]);
    fputs(", and RHS is ", stderr);
    write_type_err(types[1]);
    fputs(".\n", stderr);

    return false;
}

static bool ensure_both_numeric(const char *op, size_t line, const UCLType *_Nonnull types[2]) {

    bool ret = true;
    if (!is_numeric(types[0])) [[clang::unlikely]] {
        fprintf(
            stderr,
            "Error [line %zu]: %s requires numeric operands, but LHS is ",
            line,
            op
        );
        write_type_err(types[0]);
        fputs(".\n", stderr);
        ret = false;
    }
    if (!is_numeric(types[1])) [[clang::unlikely]] {
        fprintf(stderr, "Erorr [line %zu]: %s requires numeric operands, but RHS is ", line, op);
        write_type_err(types[0]);
        fputs(".\n", stderr);
        ret = false;
    }
    return ret;
}


static enum ucl_type_tag bin_larger_numeric(
    const char *op, size_t line, const UCLType *_Nonnull types[2]
) {
    if (ensure_numeric(op, types[0], line) && ensure_numeric(op, types[1], line))
        [[clang::likely]] {
        return types[0]->tag >= types[1]->tag ? types[0]->tag : types[1]->tag;
    }

    return TYPE_UNINHABITED;
}

static bool typeck_bin(TypeckCtx ctx, UCLExpression *_Nonnull expr) {
    REQUIRE(expr->tag == XPR_BINARY);

    if (!typeck_expr(ctx, expr->binary.operands[0])) return false;
    REQUIRE(expr->binary.operands[0]->type != nullptr);

    if (!typeck_expr(ctx, expr->binary.operands[1])) return false;
    REQUIRE(expr->binary.operands[1]->type != nullptr);

    UCLType *_Nonnull types[2] = {
        expr->binary.operands[0]->type,
        expr->binary.operands[1]->type,
    };

    switch (expr->binary.op) {
        case TOKEN_PLUS: TODO();
        case TOKEN_MINUS: TODO();
        case TOKEN_MUL: TODO();
        case TOKEN_SIGNED_MUL: TODO();
        case TOKEN_DIV: TODO();
        case TOKEN_MOD: TODO();
        case TOKEN_SIGNED_DIV: TODO();
        case TOKEN_SIGNED_MOD: TODO();
        case TOKEN_EQ: TODO();
        case TOKEN_NE: TODO();
        case TOKEN_LT: TODO();
        case TOKEN_GT: TODO();
        case TOKEN_LE: TODO();
        case TOKEN_GE: TODO();
        case TOKEN_SIGNED_LT: TODO();
        case TOKEN_SIGNED_GT: TODO();
        case TOKEN_SIGNED_LE: TODO();
        case TOKEN_SIGNED_GE: TODO();
        case TOKEN_LOGICAL_AND: TODO();
        case TOKEN_LOGICAL_OR: TODO();
        case TOKEN_BIT_NOT: TODO();
        case TOKEN_BIT_AND: TODO();
        case TOKEN_BIT_OR: TODO();
        case TOKEN_BIT_XOR: TODO();
        case TOKEN_SHR_LOG: TODO();
        case TOKEN_SHR_ARITH: TODO();
        case TOKEN_SHL: TODO();
        default:
            UNREACHABLE("Parser would not have generated AST with invalid op");
    }
}

bool typeck_expr(TypeckCtx ctx, UCLExpression *_Nonnull expr) {
    switch (expr->tag) {
        case XPR_VAR: // handled by type_of_binding in parser
            [[fallthrough]];
        case XPR_LITERAL: REQUIRE(expr->type != NULL); return true;
        case XPR_UNARY: return typeck_unary(ctx, expr);
        case XPR_BINARY: TODO();
        case XPR_FUNC_CALL: TODO();
        case XPR_GROUP: TODO();
        case XPR_TYPECAST: TODO();
        case XPR_ARRAY: TODO();
        case XPR_ASSIGN: TODO();
        case XPR_INDEX: TODO();
    }
}

static bool typeck_cond(const UCLExpression *expr) {
    REQUIRE(expr->type != NULL);
    if (!(is_numeric(expr->type) || expr->type->tag == TYPE_POINTER)) {
        fputs("Error: type ", stderr);
        write_type_err(expr->type);
        fputs(" is not a valid conditional.\n", stderr);
        return false;
    }
    return true;
}

bool typeck_stmt(TypeckCtx ctx, Statement *_Nonnull stmt) {
    switch (stmt->tag) {
        case STMT_UNPARSABLE: return false;

        case STMT_BLOCK: {
            bool ret = true;
            for (size_t i = 0; i < stmt->block->size; i++) {
                if (!typeck_stmt(ctx, &stmt->block->statements[i])) ret = false;
            }
            return ret;
        }

        case STMT_IF: {
            UCLExpression *_Nonnull cond = stmt->conditional.cond;
            if (!(typeck_expr(ctx, cond) && typeck_cond(cond))) return false;
            if (!typeck_stmt(ctx, stmt->conditional.if_body)) return false;
            if (stmt->conditional.else_body) {
                return typeck_stmt(ctx, stmt->conditional.else_body);
            }
            return true;
        }

        case STMT_WHILE: TODO();
        case STMT_FOREACH: TODO();
        case STMT_RETURN: TODO();
        case STMT_EXPR: TODO();
        case STMT_BREAK: TODO();
        case STMT_VAR_DEC: TODO();
        case STMT_FUN_DEC: TODO();
        case STMT_EXTERN_VAR: TODO();
        case STMT_EXTERN_FUN: TODO();
        case STMT_SYSCALL: TODO();
        case STMT_END: return true;
    }
}
