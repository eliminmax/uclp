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
#include "printerr.h"
#include "safety.h"
#include "table.h"
#include "todo.h"
#include "typeck.h"

typedef UCLType *_Nullable (*UnaryResolver)(TypeckCtx, UCLExpression *_Nonnull);
typedef UCLType *_Nullable (*BinaryResolver)(
    TypeckCtx, UCLExpression *_Nonnull, UCLExpression *_Nonnull
);

// Approximation of digits in SIZE_MAX, adapted from chux's StackOverflow answer
// (see https://stackoverflow.com/a/43788824)
static constexpr size_t MAX_SIZE_DIGITS =
    ((sizeof(size_t) * CHAR_BIT) * 28 / 93 + 2);

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

void write_type(const UCLType *_Nonnull type, CharBuffer *buf) {
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

UCLType *_Nonnull array_of(
    TypeckCtx ctx, size_t len, UCLType *_Nonnull mem_type
) {
    CharBuffer name = {};
    write_type(mem_type, &name);
    char numbuf[MAX_SIZE_DIGITS + 3];
    size_t sz = sprintf(numbuf, "[%zu]", len);
    append_chars(&name, sz, (uchar *)numbuf);
    String key = as_string(name);
    UCLType *type = ht_lookup(ctx.types, key);
    if (type) {
        free(name.mem);
    } else {
        type = checked_malloc(sizeof(UCLType));
        type->tag = TYPE_ARRAY;
        type->array.len = len;
        type->array.member_type = mem_type;
        ht_set(ctx.types, key, type);
        group_claim(ctx.ag, name.mem);
        group_claim(ctx.ag, type);
    }
    return type;
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
    free(buf.mem);
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
    REQUIRE(unary->operand->type != NULL);
    UCLType *_Nonnull inner_type = unary->operand->type;
    switch (unary->op) {
        case TOKEN_REF:
            fputs(
                "Warning: '.&' syntax is deprecated - use plain '&'.\n", stderr
            );
            [[fallthrough]];
        case TOKEN_BIT_AND: return expr->type = pointer_to(ctx, inner_type);

        case TOKEN_LOGICAL_NOT:
            if (!is_numeric(inner_type) && inner_type->tag != TYPE_POINTER)
                [[clang::unlikely]] {
                fputs("Error: logical can't be perfomed on ", stderr);
                write_type_err(inner_type);
                fputs(
                    ". (Supported types are integer types and pointers).\n",
                    stderr
                );
                return false;
            }
            expr->type = ht_lookup(ctx.types, (String){2, "i8"});

        case TOKEN_BIT_NOT:
            if (!ensure_numeric("bitwise not", expr->type, 0)) return false;
            expr->type = inner_type;
            return true;

        case TOKEN_MINUS:
            if (!ensure_numeric("unary negation", inner_type, 0)) return false;
            expr->type = inner_type;
            return true;

        case TOKEN_SIGNED_BITCAST_8:
        case TOKEN_UNSIGNED_BITCAST_8:
            if (!ensure_numeric("bitcast", inner_type, 0)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){2, "i8"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_16:
        case TOKEN_UNSIGNED_BITCAST_16:
            if (!ensure_numeric("bitcast", inner_type, 0)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){3, "i16"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_32:
        case TOKEN_UNSIGNED_BITCAST_32:
            if (!ensure_numeric("bitcast", inner_type, 0)) return false;
            if (!expr->type) {
                expr->type = ht_lookup(ctx.types, (String){3, "i32"});
            }
            return true;

        case TOKEN_SIGNED_BITCAST_64:
        case TOKEN_UNSIGNED_BITCAST_64:
            if (!ensure_numeric("bitcast", inner_type, 0)) return false;
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

static bool typeck_bin(TypeckCtx ctx, UCLExpression *_Nonnull expr) {
    REQUIRE(expr->tag == XPR_BINARY);
    REQUIRE(expr->binary.lhs->type != NULL);
    REQUIRE(expr->binary.rhs->type != NULL);

    UCLType *_Nonnull lhs = expr->binary.lhs->type;
    UCLType *_Nonnull rhs = expr->binary.rhs->type;

    switch (expr->binary.op) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            if ((is_numeric(lhs) && rhs->tag == TYPE_POINTER) ||
                (lhs->tag == TYPE_POINTER && is_numeric(rhs))) {
                TODO("Pointer arithmetic not yet implemented");
            }
            [[fallthrough]];
        case TOKEN_MUL:
        case TOKEN_SIGNED_MUL:
        case TOKEN_DIV:
        case TOKEN_MOD:
        case TOKEN_SIGNED_DIV:
        case TOKEN_SIGNED_MOD:
            if (!is_numeric(lhs)) [[clang::unlikely]] {
                fputs("Can't perform arithmetic on ", stderr);
                write_type_err(lhs);
                fputs(".\n", stderr);
                return false;
            }
            if (lhs != rhs) [[clang::unlikely]] { goto mismatch; }
            expr->type = lhs;
            return true;

        case TOKEN_EQ:
        case TOKEN_NE:
            if (lhs != rhs) [[clang::unlikely]] { goto mismatch; }
            switch (lhs->tag) {
                case TYPE_POINTER:
                case TYPE_I8:
                case TYPE_I16:
                case TYPE_I32:
                case TYPE_I64:
                    goto return_bool;

                [[clang::unlikely]] default:
                    fputs("Equality checks not supported for ", stderr);
                    write_type_err(lhs);
                    fputs(".\n", stderr);
                    return false;
            }

        case TOKEN_LT:
        case TOKEN_GT:
        case TOKEN_LE:
        case TOKEN_GE:
        case TOKEN_SIGNED_LT:
        case TOKEN_SIGNED_GT:
        case TOKEN_SIGNED_LE:
        case TOKEN_SIGNED_GE:
            if (lhs != rhs) {
                if (lhs->tag != TYPE_POINTER || rhs->tag != TYPE_POINTER)
                    [[clang::unlikely]] {
                    fputs(
                        "Can only order 2 integers of the same size or 2 "
                        "pointers.",
                        stderr
                    );
                    return false;
                }
            }

            expr->type = ht_lookup(ctx.types, (String){2, "i8"});
            REQUIRE(expr->type);
            return true;

        case TOKEN_LOGICAL_AND:
        case TOKEN_LOGICAL_OR: {
            if ((is_numeric(lhs) || lhs->tag == TYPE_POINTER)) {
                if (is_numeric(rhs) || rhs->tag == TYPE_POINTER) {
                    expr->type = rhs;
                    return true;
                } else {
                    fputs("Can't use ", stderr);
                    write_type_err(rhs);
                    fputs(" as a boolean value.\n", stderr);
                    return false;
                }
            } else {
                fputs("Can't use ", stderr);
                write_type_err(lhs);
                fputs(" as a boolean value.\n", stderr);
                return false;
            }
        }

        case TOKEN_BIT_NOT:
        case TOKEN_BIT_AND:
        case TOKEN_BIT_OR:
        case TOKEN_BIT_XOR:
            if (!is_numeric(lhs)) {
                fputs("Can't perform bitwise operations on ", stderr);
                write_type_err(lhs);
                fputs(".\n", stderr);
                return false;
            }
            if (lhs != rhs) [[clang::unlikely]] { goto mismatch; }
            expr->type = lhs;
            return true;

        case TOKEN_SHR_LOG:
        case TOKEN_SHR_ARITH:
        case TOKEN_SHL:
            if (!is_numeric(lhs)) {
                fputs("Can't perform bit shift operations on ", stderr);
                write_type_err(lhs);
                fputs(".\n", stderr);
                return false;
            }
            if (!is_numeric(rhs)) {
                fputs("Shift amount must be a number, not ", stderr);
                write_type_err(rhs);
                fputs(".\n", stderr);
                return false;
            }
            expr->type = lhs;
            return true;
        default:
            UNREACHABLE("Parser would not have generated AST with invalid op");
    }

return_bool:
    expr->type = ht_lookup(ctx.types, (String){2, "i8"});
    REQUIRE(expr->type);
    return true;

mismatch:
    fputs(
        "Operator requires LHS and RHS to have the same type, but LHS is ",
        stderr
    );
    write_type_err(lhs);
    fputs(", and RHS is ", stderr);
    write_type_err(rhs);
    fputs(".\n", stderr);
    return false;
}

bool typeck_expr(TypeckCtx ctx, UCLExpression *_Nonnull expr) {
    switch (expr->tag) {
        case XPR_VAR: // handled by type_of_binding in parser
            [[fallthrough]];
        case XPR_LITERAL: REQUIRE(expr->type != NULL); return true;
        case XPR_UNARY: return typeck_unary(ctx, expr);
        case XPR_BINARY: return typeck_bin(ctx, expr);
        case XPR_FUNC_CALL: TODO();
        case XPR_GROUP:
            REQUIRE(expr->grouped->type != NULL);
            expr->type = expr->grouped->type;
            return true;
        case XPR_TYPECAST: {
            REQUIRE(expr->type != NULL);

            // TODO: Check cast compatibility
            return true;
        }
        case XPR_ARRAY: {
            auto array = expr->array;
            REQUIRE(array->size > 0);

            if (array->type && array->vals[0]->type != array->type) {
                fputs("Error: array declared as ", stderr);
                write_type_err(array->type);
                fputs(" contains ", stderr);
                write_type_err(array->vals[0]->type);
                fputs(".\n", stderr);
                return false;
            }

            for (size_t i = 0; i < array->size; i++) {
                if (i && array->vals[i]->type != array->vals[0]->type) {
                    fputs("Error: array contains both ", stderr);
                    write_type_err(array->vals[0]->type);
                    fputs(" and ", stderr);
                    write_type_err(array->vals[i]->type);
                    fputs(".\n", stderr);
                    return false;
                }
            }
            expr->type = array_of(ctx, array->size, array->type);
        }

        case XPR_ASSIGN:
            auto assign = expr->assign;
            // TODO: check assignment type compatibility
            expr->type = assign.place->type;
            return true;
        case XPR_INDEX:
            auto index = expr->index;
            // TYPE
            if (!is_numeric(index.index->type)) {
                fputs("Error: can't use non-numeric type ", stderr);
                write_type_err(index.index->type);
                fputs("to index an array.\n", stderr);
                return false;
            }

            // ERROR MESSAGE
            switch (index.array->type->tag) {
                case TYPE_POINTER:
                    if (!(expr->type = index.array->type->pointee_type)) {
                        fputs(
                            "Error: can't index a dynamic pointer.\n", stderr
                        );
                        return false;
                    }
                    return true;

                case TYPE_ARRAY:
                    expr->type = index.array->type->array.member_type;
                    return true;

                [[clang::unlikely]] default: {
                    fputs("Error: can't index ", stderr);
                    write_type_err(index.array->type);
                    fputs(".\n", stderr);
                    return false;
                }
            }

            expr->type = index.array->type->array.member_type;
            return true;
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

bool typeck_stmt(TypeckCtx, Statement *_Nonnull stmt) {
    switch (stmt->tag) {
        case STMT_UNPARSABLE: return false;

        case STMT_BLOCK: return true;

        case STMT_IF: return typeck_cond(stmt->conditional.cond);

        case STMT_WHILE: return typeck_cond(stmt->while_loop.cond);
        // currently type checked within parser to properly infer variable type
        case STMT_FOREACH: return true;
        case STMT_RETURN: TODO();
        case STMT_EXPR: assert(stmt->expr->type != NULL); return true;
        case STMT_BREAK: TODO();
        case STMT_VAR_DEC: assert(stmt->declaration.type != NULL); return true;
        case STMT_FUN_DEC: TODO();
        case STMT_EXTERN_VAR: TODO();
        case STMT_EXTERN_FUN: TODO();
        case STMT_SYSCALL: TODO();
        case STMT_END: return true;
    }
}
