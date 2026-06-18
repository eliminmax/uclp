/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_TU_H
#define UCLP_TU_H

#define TU_ENUM(name, type, ...) \
    enum [[clang::enum_extensibility(closed)]] : type { \
        __VA_ARGS__ \
    } name [[clang::require_explicit_initialization]]
#endif /* UCLP_TU_H */
