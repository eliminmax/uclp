/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_TYPECK_H
#define UCLP_TYPECK_H

#include "ast.h"
#include "lang/types.h"
#include "mem/dynarray.h"
#include "table.h"

Alignment ucl_alignof(const UCLType *_Nonnull type);
void write_type(const UCLType *_Nonnull t, CharBuffer *_Nonnull b);

inline bool is_numeric(const UCLType *_Nonnull type) {
    switch (type->tag) {
        case TYPE_UNINHABITED:
        case TYPE_FUNCTION:
        case TYPE_POINTER:
        case TYPE_OPAQUE:
        case TYPE_ARRAY: return false;
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64: return true;
    }
}

typedef struct { HashTable types; AllocGroup ag; } TypeckCtx;

bool typeck_expr(TypeckCtx ctx, UCLExpression *_Nonnull expr);
bool typeck_stmt(TypeckCtx ctx, Statement *_Nonnull stmt);

#endif
