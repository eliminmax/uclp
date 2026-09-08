/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdarg.h>

#include "annotations.h"
#include "colorize.h"
#include "printerr.h"
#include "safety.h"

#define MAX_ERR_TOKEN_SIZE 0x1000

void ucl_verror(
    size_t line,
    const uchar *_Nonnull source,
    size_t source_size,
    const uchar *_Nonnull start,
    size_t span_size,
    const char *_Nonnull fmt,
    va_list fmt_args
) {
    const uchar NODEREF *const _Nonnull source_end = &source[source_size];
    REQUIRE(start + span_size <= source_end);
    REQUIRE(*start < 0x80);

    const uchar *line_start = start;
    const uchar *line_end = start + span_size;

    while (line_start > source && line_start[-1] != '\n') line_start--;
    while (line_end < source_end && *line_end != '\n') line_end++;

    static const char *ERR_HL, *HL_END;
    // bold; red; underlined
    if (!ERR_HL) ERR_HL = select_color(stderr, "\x1b[1;31;4m");
    if (!HL_END) HL_END = select_color(stderr, "\x1b[m");

    ptrdiff_t prefix_len = start - line_start;
    ptrdiff_t suffix_len = line_end - &start[span_size];

    if (*ERR_HL) fputs(ERR_HL, stderr);
    vfprintf(stderr, fmt, fmt_args);
    if (*HL_END) fputs(HL_END, stderr);

    if (span_size > MAX_ERR_TOKEN_SIZE || prefix_len > MAX_ERR_TOKEN_SIZE ||
        suffix_len > MAX_ERR_TOKEN_SIZE) {
        fprintf(stderr, "  (line %zu)", line);
        return;
    }

    fprintf(stderr, "  (line %zu):\n", line);
    // dim italics
    write_color(line_start, stderr, prefix_len, "2;3");
    // bold black on yellow
    write_color(start, stderr, span_size, "1;43;30");
    write_color(&start[span_size], stderr, suffix_len, "2;3");
    fputc('\n', stderr);
}

extern inline void ucl_error(
    size_t,
    const uchar *_Nonnull,
    size_t,
    const uchar *_Nonnull,
    size_t,
    const char *_Nonnull,
    ...
);
