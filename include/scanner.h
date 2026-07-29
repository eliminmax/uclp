/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "token.h"

typedef struct scanner *_Nonnull Scanner;

[[clang::acquire_handle("scanner")]]
Scanner start_scanner(String source);
Token next_token([[clang::use_handle("scanner")]] Scanner);
void free_scanner([[clang::release_handle("scanner")]] Scanner);
