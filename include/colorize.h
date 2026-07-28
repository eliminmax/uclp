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

typedef void (*ColorWriter)(const uchar *text, FILE *, int len, const char *color_code);

enum color_setting {
    COLOR_DEFAULT,
    COLOR_NEVER,
    COLOR_ALWAYS,
};

extern ColorWriter write_color;
extern enum color_setting COLOR_SETTING;

#endif
