/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UNREACHABLE_H
#define UNREACHABLE_H

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif /* __has_builtin */

#if ! __has_builtin(__builtin_unreachable) 
#ifdef SKIP_UNREACHABLE_CHECKS
#warning "__builtin_unreachable not available - can't skip UNREACHABLE"
#undef SKIP_UNREACHABLE_CHECKS
#endif /* SKIP_UNREACHABLE_CHECKS */
#endif /* ! __has_builtin(__builtin_unreachable) */

#ifndef SKIP_UNREACHABLE_CHECKS
#if __has_builtin(__builtin_trap)
#define TRAP() __builtin_trap()
#else
#include <stdlib.h> /* IWYU pragma: keep */
#define TRAP() abort()
#endif /* __has_builtin(__builtin_unreachable) */

#include <stdio.h> /* IWYU pragma: keep */
#define UNREACHABLE() \
    do { \
        fprintf( \
            stderr, \
            "INVALID STATE REACHED in %s:%s (line %u)\n", \
            __FILE__, \
            __func__, \
            __LINE__ \
        ); \
        TRAP(); \
    } while (0)

#endif /* SKIP_UNREACHABLE_CHECKS */
#endif /* UNREACHABLE_H */
