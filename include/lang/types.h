/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * C representations of UCL types
 */

#ifndef UCLP_TYPES_H
#define UCLP_TYPES_H

#include <stddef.h>
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

typedef enum Alignment {
    ALIGN_DYNAMIC = 0,
    ALIGN_8 = 8,
    ALIGN_16 = 16,
    ALIGN_32 = 32,
    ALIGN_64 = 64,
    ALIGN_PTR = ALIGN_64,
} Alignment;

// A struct which represents a pointer to the underlying type
struct [[deprecated]] ucl_pointer {
    struct ucl_type *_Nullable pointee_type;
    [[deprecated]]
    Alignment alignment;
};

// a struct which represents an opaque type's size and alignment
struct ucl_opaque_type {
    size_t size;

    Alignment alignment;
};

// a struct which represents an array's type and length
struct ucl_array {
    struct ucl_type *_Nonnull member_type;
    size_t len;
};

struct ucl_functype {
    struct ucl_type *_Nullable return_type;
    uint8_t nargs;
    struct ucl_type [[clang::counted_by(nargs)]] *_Nonnull arg_types[];
};

// A tagged union representing a type
typedef struct ucl_type {
    enum ucl_type_tag : uint8_t {
        TYPE_UNINHABITED,
        TYPE_POINTER,
        TYPE_OPAQUE,
        TYPE_I8,
        TYPE_I16,
        TYPE_I32,
        TYPE_I64,
        TYPE_ARRAY,
        TYPE_FUNCTION,
    } tag;

    union {
        struct ucl_type *_Nullable pointee_type;
        struct ucl_opaque_type opaque;
        struct ucl_array array;
        struct ucl_functype *_Nonnull func;
    };
} UCLType;

#endif /* UCLP_TYPES_H */
