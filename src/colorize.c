/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "colorize.h"

enum color_setting COLOR_SETTING = COLOR_DEFAULT;

static void write_color_resolve(const uchar *, FILE *, int, const char *);
static const char *select_color_resolve(FILE *, const char *);
ColorWriter write_color = write_color_resolve;
ColorFilt select_color = select_color_resolve;

static void write_color_real(
    const uchar *text, FILE *out, int len, const char *code
) {
    fprintf(out, "\x1b[%sm%.*s\x1b[m", code, len, text);
}

static const char *select_color_real(FILE *, const char * code) {
    return code;
}

static void write_color_ignore(
    const uchar *text, FILE *out, int len, const char *
) {
    fprintf(out, "%.*sm", len, text);
}

static const char *select_color_ignore(FILE*, const char *) {
    return "";
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

static const char *select_color_if_tty(FILE *dest, const char *code) {
    if (isatty(fileno(dest))) return code;
    return "";
}

static void resolve_color_funcs() {
    switch (COLOR_SETTING) {
        case COLOR_DEFAULT: {
            char *nc = getenv("NO_COLOR");
            // check if nc is set to a non-empty string
            if (nc && *nc) {
                write_color = write_color_ignore;
                select_color = select_color_ignore;
            } else {
                write_color = write_color_if_tty;
                select_color = select_color_if_tty;
            }
            break;
        }
        case COLOR_NEVER:
            write_color = write_color_ignore;
            select_color = select_color_if_tty;
            break;
        case COLOR_ALWAYS:
            write_color = write_color_real;
            select_color = select_color_real;
            break;
    }
}

static void write_color_resolve(
    const uchar *text, FILE *out, int len, const char *color_code
) {
    resolve_color_funcs();
    write_color(text, out, len, color_code);
}

static const char *select_color_resolve(FILE *dest, const char *code) {
    resolve_color_funcs();
    return select_color(dest, code);
}
