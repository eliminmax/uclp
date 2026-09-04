/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_PARSER
#define UCLP_PARSER

#include "ast.h"
#include "sized_string.h"

typedef struct parser *_Nonnull Parser;

[[clang::acquire_handle("parser")]]
Parser parser(String source);
Statement get_statement([[clang::use_handle("parser")]] Parser);
void free_parser([[clang::release_handle("parser")]] Parser);

#endif
