/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * C representations of UCL types
 */

#include <stdint.h>

// A union which represents an 8-bit sign-agnostic integer
typedef union {
    // value as signed integer
    int8_t s;
    // value as unsigned integer
    uint8_t u;
} ucl_i8;

// A union which represents a 16-bit sign-agnostic integer
typedef union {
    // value as signed integer
    int16_t s;
    // value as unsigned integer
    uint16_t u;
} ucl_i16;

// A union which represents a 32-bit sign-agnostic integer
typedef union {
    // value as signed integer
    int32_t s;
    // value as unsigned integer
    uint32_t u;
} ucl_i32;

// A union which represents a 64-bit sign-agnostic integer
typedef union {
    // value as signed integer
    int64_t s;
    // value as unsigned integer
    uint64_t u;
} ucl_i64;
