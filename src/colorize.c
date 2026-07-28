/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "colorize.h"

static void write_color_real(
    const uchar *text, FILE *out, int len, const char *code
) {
    fprintf(out, "\x1b[%sm%.*s\x1b[m", code, len, text);
}

static void write_color_ignore(
    const uchar *text, FILE *out, int len, const char *
) {
    fprintf(out, "%.*sm", len, text);
}

static void write_color_if_tty(
    const uchar *text, FILE *out, int len, const char *code
) {
    if (isatty(fileno(out))) {
        write_color_real(text, out, len, code);
    } else {
        write_color_ignore(text, out, len, code);
    }
}

static void write_color_resolve(const char *, FILE *, int, const char *);

ColorWriter write_color = write_color_resolve;
enum color_setting COLOR_SETTING = COLOR_DEFAULT;

static void write_color_resolve(
    const uchar *text, FILE *out, int len, const char *color_code
) {
    switch (COLOR_SETTING) {
        case COLOR_DEFAULT: {
            char *nc = getenv("NO_COLOR");
            // check if nc is set to a non-empty string
            if (nc && *nc) {
                write_color = write_color_ignore;
            } else {
                write_color = write_color_if_tty;
            }
            break;
        }
        case COLOR_NEVER:
            write_color = write_color_ignore;
            break;
        case COLOR_ALWAYS:
            write_color = write_color_real;
            break;
    }

    write_color(text, out, len, color_code);
}
