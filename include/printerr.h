/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdarg.h>

#include <stddef.h>
#include "typenames.h"

// print an error message, highlighting the source of the error
[[gnu::cold, gnu::format(__printf__, 6, 0)]]
void ucl_verror(
    size_t line,
    const uchar *_Nonnull source,
    size_t len,
    const uchar *_Nonnull start,
    size_t span_size,
    const char *_Nonnull fmt,
    va_list fmt_args
);

// thin wrapper around ucl_verror that handles the variadics
[[gnu::cold, gnu::format(__printf__, 6, 7)]]
inline void ucl_error(
    size_t line,
    const uchar *_Nonnull source,
    size_t len,
    const uchar *_Nonnull start,
    size_t span_size,
    const char *_Nonnull fmt,
    ...
) {
    va_list fmt_args;
    va_start(fmt_args, fmt);

    ucl_verror(line, source, len, start, span_size, fmt, fmt_args);

    va_end(fmt_args);
}
