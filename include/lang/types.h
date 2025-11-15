/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
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

// A struct which represents a pointer to the underlying type
struct ucl_pointer {
    struct ucl_type *_Nullable pointee_type;

    enum {
        PTR_ALIGN_DYNAMIC = 0,
        PTR_ALIGN_8 = 8,
        PTR_ALIGN_16 = 16,
        PTR_ALIGN_32 = 32,
        PTR_ALIGN_64 = 64
    } alignment;
};

// a struct which represents an opaque type's size and alignment
struct ucl_opaque_type {
    size_t size;

    enum {
        OPAQUE_ALIGN_8 = 8,
        OPAQUE_ALIGN_16 = 16,
        OPAQUE_ALIGN_32 = 32,
        OPAQUE_ALIGN_64 = 64
    } alignment;
};

// a struct which represents an array's type and length
struct ucl_array {
    struct ucl_type *_Nonnull member_type;
    size_t len;
};

// A tagged union representing a type and its metadata
typedef struct ucl_type {
    enum ucl_type_tag: uint8_t {
        TYPE_POINTER,
        TYPE_OPAQUE,
        TYPE_I8,
        TYPE_I16,
        TYPE_I32,
        TYPE_I64,
        TYPE_ARRAY
    } tag;

    enum : uint8_t { STORAGE_STACK, STORAGE_STATIC } storage;

    union {
        struct ucl_pointer ptr;
        struct ucl_opaque_type opaque;
        struct ucl_array array;
    };
} UCLType;

#endif /* UCLP_TYPES_H */
