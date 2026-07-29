/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_COLORIZE
#define UCLP_COLORIZE
#include <stdint.h>
#include <stdio.h>

#include "typenames.h"

typedef void (*_Nonnull ColorWriter)(
    const uchar *_Nonnull text,
    FILE *_Nonnull,
    int len,
    const char *_Nonnull color_code
);

typedef const char *_Nonnull (*_Nonnull ColorFilt)(
    FILE *_Nonnull dest, const char *_Nonnull color_code
);

enum color_setting {
    COLOR_DEFAULT,
    COLOR_NEVER,
    COLOR_ALWAYS,
};

extern ColorWriter write_color;
extern ColorFilt select_color;
extern enum color_setting COLOR_SETTING;

#endif
