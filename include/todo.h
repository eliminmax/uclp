/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

#ifndef UCLP_TODO_H
#define UCLP_TODO_H
#include <stdio.h> // IWYU pragma: keep

#ifndef IGNORE_TODO
[[deprecated]]
#endif
// an empty NO-OP function called within the TODO macro
// marked as deprecated to trigger warnings wherever the TODO macro is used
[[gnu::artificial]] static inline void TODO(...) {}

// macro that does the following:
//
// 1. pass all variadic arguments to an always-deprecated no-op function, to
//    trigger compile-time warnings wherever it's used
// 2. print an error message pointing to where it was invoked
// 3. call __builtin_trap
#define TODO(...) \
    do { \
        TODO(__VA_ARGS__); \
        fprintf( \
            stderr, "TODO: [%s:%d] in %s\n", __FILE_NAME__, __LINE__, __func__ \
        ); \
        __builtin_trap(); \
    } while (0)
#endif
