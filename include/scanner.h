/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "string_manager.h"
#include "token.h"
#include "typenames.h"

typedef struct scanner *_Nonnull Scanner;

[[clang::acquire_handle("scanner")]]
Scanner start_scanner(
    String source, [[clang::use_handle("pool")]] StringPool ident_pool
);
Token next_token([[clang::use_handle("scanner")]] Scanner);
void free_scanner([[clang::release_handle("scanner")]] Scanner);

const uchar *_Nonnull get_position([[clang::use_handle("scanner")]] Scanner s);
String get_source([[clang::use_handle("scanner")]] Scanner s);
