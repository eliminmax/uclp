/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_SAFETY_H
#define UCLP_SAFETY_H

#include <assert.h> // IWYU pragma: export

#ifdef NDEBUG
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if __has_builtin(__builtin_assume)
#define REQUIRE(cond) __builtin_assume(cond)
#else
#define REQUIRE(cond) ((void)0)
#endif // __has_builtin(__builtin_assume)

#define UNREACHABLE(comment) unreachable()
#else

#include <stdio.h> // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep

// An alternative to `assert` with more information provided
//
// If NDEBUG is defined, it tries to hint to the compiler to optimize based on
// the condition.
#define REQUIRE(cond) \
    do { \
        if (!(cond)) { \
            fputs("INTERNAL COMPILER ERROR!\n", stderr); \
            fprintf( \
                stderr, \
                "The internal invariant `" #cond "` did not hold (" __FILE__ \
                ":%d).\n", \
                __LINE__ \
            ); \
            abort(); \
        } \
    } while (false)

#define UNREACHABLE(comment) \
    do { \
        fputs("\"Unreachable\" code reached in compiler!\n", stderr); \
        fprintf(stderr, "Reason it was thought unreachable: %s\n", comment); \
        abort(); \
    } while (false)

#endif // NDEBUG
#endif // UCLP_SAFETY_H
