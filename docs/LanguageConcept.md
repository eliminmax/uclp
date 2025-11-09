<!--
SPDX-FileCopyrightText: 2025 Eli Array Minkoff

SPDX-License-Identifier: 0BSD
-->

# Language Overview

<!-- vim-markdown-toc GFM -->

* [Type System](#type-system)
    * [Basic Integral Types](#basic-integral-types)
    * [Arrays](#arrays)
    * [Pointers](#pointers)
    * [External types](#external-types)
* [Program Structure + Control Flow](#program-structure--control-flow)
* [Syntax](#syntax)
    * [Comments](#comments)
    * [Integer Literals](#integer-literals)
    * [Variable Declarations](#variable-declarations)
    * [Function Declarations](#function-declarations)
    * [Conditionals](#conditionals)
    * [While Loop](#while-loop)
* [Integer Operations](#integer-operations)
    * [Overview and Syntax](#overview-and-syntax)
    * [Specific Operations](#specific-operations)
        * [Logical Not](#logical-not)
        * [Bitwise Not](#bitwise-not)
            * [Examples](#examples)
        * [Negation](#negation)
        * [Unsigned Cast to N Bits](#unsigned-cast-to-n-bits)
            * [Examples](#examples-1)
        * [Signed Cast to N Bits](#signed-cast-to-n-bits)
            * [Examples](#examples-2)
        * [Unsigned Multiplication](#unsigned-multiplication)
        * [Unsigned Division](#unsigned-division)
        * [Unsigned Modulo](#unsigned-modulo)
        * [Signed Multiplication](#signed-multiplication)
        * [Signed Division](#signed-division)
        * [Signed Modulo](#signed-modulo)
        * [Addition](#addition)
        * [Subtraction](#subtraction)
        * [Logical Shift Right](#logical-shift-right)
        * [Arithmetic Shift Right](#arithmetic-shift-right)
        * [Shift Left](#shift-left)
        * [Equal](#equal)
        * [Not Equal](#not-equal)
        * [Unsigned Less Than](#unsigned-less-than)
        * [Unsigned Greater Than](#unsigned-greater-than)
        * [Unsigned Less or Equal](#unsigned-less-or-equal)
        * [Unsigned Greater or Equal](#unsigned-greater-or-equal)
        * [Signed Less Than](#signed-less-than)
        * [Signed Greater Than](#signed-greater-than)
        * [Signed Less or Equal](#signed-less-or-equal)
        * [Signed Greater or Equal](#signed-greater-or-equal)
        * [Bitwise And](#bitwise-and)
        * [Bitwise Or](#bitwise-or)
        * [Bitwise Xor](#bitwise-xor)
        * [Logical And](#logical-and)
        * [Logical Or](#logical-or)
* [Pointers](#pointers-1)
    * [Pointer Alignment](#pointer-alignment)
    * [Casting Pointers](#casting-pointers)
* [Arrays](#arrays-1)
* [Strings and Chars](#strings-and-chars)
* [C Interop with `dynamic`](#c-interop-with-dynamic)
    * [C Types](#c-types)
    * [Calling C Functions](#calling-c-functions)
        * [Example: `libcpuinfo`](#example-libcpuinfo)
    * [`*dynamic` Pointers](#dynamic-pointers)
        * [Example: `malloc` and `free`](#example-malloc-and-free)
* [`opaque` types](#opaque-types)

<!-- vim-markdown-toc -->

UCLP is intended to be a limited language that's easy to compile, not a particularly robust or expressive language.

When I started writing this, I had gotten the loader program for the binary format working, but the language itself was just a vague idea. This document was intended to be a place where I could flesh out the syntax and semantics, until I had the language figured out.

## Type System

The type system is intended to be minimal, and it assumes a 64-bit architecture.

### Basic Integral Types

The following types are to be supported:

* `i8`
* `i16`
* `i32`
* `i64`

They are integers with the specified size. My current thinking is that they'll be treated as unsigned by default, but can be coerced into being used as signed types for specific operations, with 2's complement representation. I am not 100% sure of that. Could be a bad idea - I'll have to see.

Conversions between integral types are always explicit.

### Arrays

Fixed-size arrays can be declared and used.

### Pointers

Pointers to the basic integral types will be supported, as will pointers to other pointers.

### External types

External types can be declared with size and alignment specified. They can only be passed to or called from foreign functions. Size must be a multiple of alignment.

## Program Structure + Control Flow

Functions, `while` loops, and `if` statements all work more-or-less how they would in C.

`foreach` loops can iterate over all elements in arrays.

## Syntax

I'm thinking of a minimal syntnax. My main programming languages of choice these days are C, Rust, Python, and AWK, so they are the main influences of the syntax.

### Comments

Comments can either start with `//` and go to the end of the line, or start with `/*` and go until `*/`

### Integer Literals

Integer literals have up to 3 components - the (optional) base modifier prefix, the value, and the (optional) type suffix. There is no whitespace within them.

If no base modifier prefix is present, then the integer value is written in decimal. The base modifiers supported are `0x` for hexadecimal, `0b` for binary, or `0o` for octal.

If a single underscore (`_`) appears between digits, it's ignored, as a digit separator.

The type suffix is one of `#8`, `#16`, `#32`, or `#64`, for 8-bit, 16-bit, 32-bit, or 64-bit integers. If not specified, 32-bit is assumed.

### Variable Declarations

Variables are declared with the following formats:

```uclp
let NAME: TYPE;
let NAME = VALUE;
let NAME: TYPE = VALUE;
```

### Function Declarations

A function which takes no arguments and returns no value is declared with the following syntax:

```uclp
func NAME() {
    // ...
}
```

A function which takes an argument and returns a value is declared with the following syntax:

```uclp
func NAME(ARG_LIST) -> TYPE {
    // ...
}
```

*(`ARG_LIST` is a comma-separated list of arguments specified as `TYPE NAME`, like C.)*

For example, a function called "add_i8s" that adds 2 `i8`s, returning their sum could be:

```uclp
func add_i8s(i8 a, i8 b) -> i8 {
    return a + b;
}
```

### Conditionals

A simple if statement without an else statement:

```uclp
if CONDITION {
    // ...
}
```

An `if-else` structure

```uclp
if CONDITION {
    // ...
} else {
    // ...
}
```

An `if-elif-else` structure:

```uclp
if CONDITION {
    // ...
} elif CONDITION {
    // ...
} else {
    // ...
}
```

### While Loop

```uclp
while CONDITION {
    // ...
}
```

## Integer Operations

There are the normal unary and binary operations that most languages have, as well as downcasts to shrink integers to a smaller size, as well as sign extensions and zero extensions to grow them.

By default, operations are assumed to follow unsigned semantics. For operations where unsigned and signed semantics differ, a signed variant exists, using the `@` sign at the start of the operator.

### Overview and Syntax

| Operation                 | Syntax     | Priority |
|---------------------------|------------|----------|
| Logical Not               | `!a`       | 1        |
| Bitwise Not               | `~a`       | 1        |
| Negation                  | `-a`       | 1        |
| Unsigned Cast to N Bits   | `:[N]a`    | 1        |
| Signed Cast to N Bits     | `@:[N]a`   | 1        |
| Unsigned Multiplication   | `a * b`    | 2        |
| Unsigned Division         | `a / b`    | 2        |
| Unsigned Modulo           | `a % b`    | 2        |
| Signed Multiplication     | `a @* b`   | 2        |
| Signed Division           | `a @/ b`   | 2        |
| Signed Modulo             | `a @% b`   | 2        |
| Addition                  | `a + b`    | 3        |
| Subtraction               | `a - b`    | 3        |
| Bitwise And               | `a & b`    | 4        |
| Bitwise Or                | `a \| b`   | 4        |
| Bitwise Xor               | `a ^ b`    | 4        |
| Logical Shift Right       | `a >> b`   | 5        |
| Arithmetic Shift Right    | `a @>> b`  | 5        |
| Shift Left                | `a << b`   | 5        |
| Equal                     | `a == b`   | 6        |
| Not Equal                 | `a != b`   | 6        |
| Unsigned Less Than        | `a < b`    | 6        |
| Unsigned Greater Than     | `a > b`    | 6        |
| Unsigned Less or Equal    | `a <= b`   | 6        |
| Unsigned Greater or Equal | `a >= b`   | 6        |
| Signed Less Than          | `a @< b`   | 6        |
| Signed Greater Than       | `a @> b`   | 6        |
| Signed Less or Equal      | `a @<= b`  | 6        |
| Signed Greater or Equal   | `a @>= b`  | 6        |
| Logical And               | `a && b`   | 7        |
| Logical Or                | `a \|\| b` | 7        |

### Specific Operations

#### Logical Not

`!a` evaluates to `1` if `a` is `0`, and `0` otherwise.

#### Bitwise Not

`~a` evaluates to the bitwise inverse of `a` - i.e. it evaluates to the value produced by flipping each bit of `a` individually.

##### Examples

* `~0#8` evaluates to `0b1111_1111#8`
* `~0b1010_1010` evaluates to `0b1111_1111_1111_1111_1111_1111_0101_0101` (due to the implicit 32-bit integer width default)

#### Negation

`-a` evaluates to the 2's complement negation of `a`.

#### Unsigned Cast to N Bits

`:[N]a`, where `N` is one of `8`, `16`, `32`, or `64`, is the value of `a` converted to an `N`-bit integer type. If `a` is smaller than `N` bits, it's zero-extended to `N` bits, and if it's larger, it's truncated.

##### Examples

* `:[8]0x1234#16` evaluates to `0x34#8`
* `:[16]0xdeadbeef` evaluates to `0xbeef#16`
* `:[32]0xff#8` evaluates to `0xff#32`

#### Signed Cast to N Bits

`@:[N]a`, where `N` is one of `8`, `16`, `32`, or `64`, is the value of `a` converted to an `N`-bit integer type. If `a` is smaller than `N` bits, it's sign-extended to `N` bits, and if it's larger, it's truncated.

##### Examples

* `:[8]0x1234#16` evaluates to `0x34#8`
* `:[16]0xdeadbeef` evaluates to `0xbeef#16`
* `:[32]0xff#8` evaluates to `0xffff_ffff#32`

#### Unsigned Multiplication

`a * b` evaluates to the product of `a` and `b`, treating both as unsigned values.

#### Unsigned Division

`a / b` evaluates to the quotient of `a` divided by `b`, treating both as unsigned values.

#### Unsigned Modulo

`a % b` evaluates to the remainder of `a` divided by `b`, treating both as unsigned values.

#### Signed Multiplication

`a * b` evaluates to the product of `a` and `b`, treating both as unsigned values.

#### Signed Division

`a / b` evaluates to the quotient of `a` divided by `b`, treating both as unsigned values.

#### Signed Modulo

`a % b` evaluates to the remainder of `a` divided by `b`, treating both as unsigned values.

#### Addition

`a + b` evaluates to the sum of `a` and `b`.

#### Subtraction

`a - b` evaluates to the difference between `a` and `b`.

#### Logical Shift Right

`a >> b` moves the bits of `a` `b` spaces over to the right, dropping the lowest `b` bits, and filling the highest `b` bits with 0.

#### Arithmetic Shift Right

`a >> b` moves the bits of `a` `b` spaces over to the right, dropping the lowest `b` bits, and filling the highest `b` bits with the highest bit of `a`.

#### Shift Left

`a << b` moves the bits of `a` `b` spaces over to the right, dropping the highest `b` bits, and filling the lowest `b` bits with 0.

#### Equal

`a == b` evaluates to 1 if `a` and `b` are equal, and 0 otherwise.

#### Not Equal

`a != b` evaluates to 0 if `a` and `b` are equal, and 1 otherwise.

#### Unsigned Less Than

`a < b` evaluates to 0 if `a` is less than `b`, treating `a` and `b` as unsigned values.

#### Unsigned Greater Than

`a > b` evaluates to 0 if `a` is greater than `b`, treating `a` and `b` as unsigned values.

#### Unsigned Less or Equal

`a < b` evaluates to 0 if `a` is less than or equal to `b`, treating `a` and `b` as unsigned values.

#### Unsigned Greater or Equal

`a > b` evaluates to 0 if `a` is greater than or equal to `b`, treating `a` and `b` as unsigned values.

#### Signed Less Than

`a < b` evaluates to 0 if `a` is less than `b`, treating `a` and `b` as signed values.

#### Signed Greater Than

`a > b` evaluates to 0 if `a` is greater than `b`, treating `a` and `b` as signed values.

#### Signed Less or Equal

`a < b` evaluates to 0 if `a` is less than or equal to `b`, treating `a` and `b` as signed values.

#### Signed Greater or Equal

`a > b` evaluates to 0 if `a` is greater than or equal to `b`, treating `a` and `b` as signed values.

#### Bitwise And

`a & b` evaluates to the value produced by going bit by bit, setting the bit to 1 if the equivalent bits in `a` and `b` are 1, and 0 otherwise.

#### Bitwise Or

`a | b` evaluates to the value produced by going bit by bit, setting the bit to 0 if the equivalent bits in `a` and `b` are 0, and 1 otherwise.

#### Bitwise Xor

`a ^ b` evaluates to the value produced by going bit by bit, setting the bit to 1 if exactly 1 of the equivalent bits in `a` and `b` is 1, and 0 otherwise.

#### Logical And

`a && b` evaluates to 1 if both `a` and `b` evaluate to nonzero values.

#### Logical Or

`a || b` evaluates to 1 if either `a` or `b` evaluate to nonzero values.

## Pointers

Pointers use C-like syntax, with `*` as the dereference operator, and `&` as the address operator.

Unlike C, however, the pointer is part of the type, not the binding, so instead of declaring a pointer "i" to an `i8` with `let *i: i8;`, the syntax is `let i: *i8`. When declaring a function `foo` which takes a pointer to an `i8` in a parameter called "ptr_arg", the syntax is  `func foo(*i8 ptr_arg)`.

### Pointer Alignment

Some types must be properly aligned in memory. Specifically, an `i16` must be at an even address, an `i32` must be at a multiple of 4, and `i64`s and pointers must be at a multiple of 8.

### Casting Pointers

The `:{T}` operator converts a pointer to another type to type `T`. Casting a type with a narrower alignment to a one with a wider alignment is not allowed, nor is casting to or from non-pointer types.

## Arrays

Arrays are fixed-sized sequences of data, stored sequentially in memory. They have the syntax `[a, b, c, d]`.
The syntax for the type of an array of N elements of type `T` is `T[N]`. For example, to declare an array of 3 `i32`s, the syntax would be `i32[3]`.

To declare an array of 5 16-bit values, called "arr" and set them all to 1, the following syntax is used:

```uclp
let arr = [1#16, 1#16, 1#16, 1#16, 1#16];
```

To declare an uninitialized array of 3 8-bit values, called "uninit", the following syntax is used:

```uclp
let uninit: i8[3];
```

## Strings and Chars

Chars and strings are not a "proper" type, but instead are syntactic sugar for 8-bit numbers and 8-bit arrays, respectively.

A single ASCII character between 2 quotation marks is read as a numeric value, with the exception of `'` and `\`.

`\` can be used for escape sequences, with the following supported:

* `\t` - 9 (the ASCII encoding for Tab)
* `\n` - 10 (the ASCII encoding for Line Feed)
* `\r` - 13 (the ASCII encoding for Carriage Return)
* `\x` - the next 2 bytes are treated as a hexadecimal-encoded arbitrary byte value
    * for example: `'\xff'` is equivalent to `0xff#8`.
* `\\` - 92 (the ASCII encoding for backslash)
* `\'` - 27 (the ASCII encoding for single quote)
* `\"` - 34 (the ASCII encoding for double quote)

Strings are just arrays of 8-bit values. For convenient C interop, they will have a null byte after them in memory, but it's not part of the array.

For example, `"abcdef"` would be parsed as a 6-byte array, equivalent to `[0x61#8, 0x62#8, 0x63#8, 0x64#8, 0x65#8, 0x66#8]`.
While followed by a null byte, it's not considered part of the array, for the purposes of `foreach` loops.

## C Interop with `dynamic`

### C Types

For the purposes of C interop, booleans can be represented as `i8`s, and C integer types can be represented UCLP integer types with the same size and alignment. Additionally, `void *` pointers can be used with the special `*dynamic` pointer type.

Because C functions are called using the SYSV calling convention for the AMD64 architecture, the type mapping is as follows:

| C Type                                                                                      | UCLP Type |
|---------------------------------------------------------------------------------------------|-----------|
| `char`<br>`signed char`<br>`_Bool`<br>`unsigned char`<br>`int8_t`<br>`uint8_t`              | `i8`      |
| `short`<br>`unsigned short`<br>`int16_t`<br>`uint16_t`                                      | `i16`     |
| `int`<br>`unsigned int`<br>`int32_t`<br>`uint32_t`                                          | `i32`     |
| `long`<br>`unsigned long`<br>`long long`<br>`unsigned long long`<br>`int64_t`<br>`uint64_t` | `i64`     |


### Calling C Functions

Accessing foreign data and functions is done using a simple `dlsym`-based approach, resulting in a rather leaky abstraction.

To declare foreign functions or data, `dynamic["LIB"]` is added before the `func` or the `let` declaration, and `# "SYM"` is placed after the name of the foreign object, where "LIB" is the filename of the shared object to pass to `dlopen`, and "SYM" is the symbol within that object.

#### Example: `libcpuinfo`

To use libcpuinfo to determine the number of CPU cores in C, the following functions are used:

```c
// must be called to set things up
bool cpuinfo_initialize(void);
// returns the number of CPU cores
uint32_t cpuinfo_get_cores_count(void);
// should be called to clean things up
void cpuinfo_deinitialize(void);
```

On Debian 13, the shared object for libcpuinfo is `libcpuinfo.so.0`, so to use those functions in UCLP, the following declarations would be needed.

```uclp
dynamic["libcpuinfo.so.0"] func cpuinfo_init # "cpuinfo_initialize" () -> i8;
dynamic["libcpuinfo.so.0"] func get_core_count # "cpuinfo_get_cores_count" () -> i32;
dynamic["libcpuinfo.so.0"] func cpuinfo_deinit # "cpuinfo_deinitialize" ();
```

### `*dynamic` Pointers

A `dynamic` function can be declared with `*dynamic` as an argument type, and/or as the return type. It is implicitly convertable to or from any other pointer type at the call site of the `*dynamic` function.

#### Example: `malloc` and `free`

To use the glibc malloc functions on most Linux systems:

```uclp
dynamic["libc.so.6"] func malloc(i64 size) -> *dynamic;
dynamic["libc.so.6"] func free(*dynamic ptr);
```

## `opaque` types

For the purposes of C interop, the syntax `opaque.SIZE.ALIGNMENT` can be used as a type, where `ALIGNMENT` is a number with a value that's one of `8`, `16`, `32`, or `64`, and `SIZE` is a multiple of the number used for `ALIGNMENT`
