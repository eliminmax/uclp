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
        * [Addition](#addition)
        * [Subtraction](#subtraction)
        * [Unsigned Multiplication](#unsigned-multiplication)
        * [Unsigned Division](#unsigned-division)
        * [Unsigned Modulo](#unsigned-modulo)
        * [Signed Multiplication](#signed-multiplication)
        * [Signed Division](#signed-division)
        * [Signed Modulo](#signed-modulo)
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
        * [Logical Shift Right](#logical-shift-right)
        * [Arithmetic Shift Right](#arithmetic-shift-right)
        * [Shift Left](#shift-left)

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

Functions, while loops, and `if` statements all work more-or-less how they would in C.

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

By default, operations are assumed to follow unsigned semantics. For operations where unsigned and signed semantics differ, a signed variant exists, using the `@` sign at the start of the operator

### Overview and Syntax

| Operation                 | Syntax    |
|---------------------------|-----------|
| Logical Not               | `!a`      |
| Bitwise Not               | `~a`      |
| Negation                  | `-a`      |
| Unsigned Cast to N Bits   | `:[N]a`   |
| Signed Cast to N Bits     | `@:[N]a`  |
| Addition                  | `a + b`   |
| Subtraction               | `a - b`   |
| Unsigned Multiplication   | `a * b`   |
| Unsigned Division         | `a / b`   |
| Unsigned Modulo           | `a % b`   |
| Signed Multiplication     | `a @* b`  |
| Signed Division           | `a @/ b`  |
| Signed Modulo             | `a @% b`  |
| Equal                     | `a == b`  |
| Not Equal                 | `a != b`  |
| Unsigned Less Than        | `a < b`   |
| Unsigned Greater Than     | `a > b`   |
| Unsigned Less or Equal    | `a <= b`  |
| Unsigned Greater or Equal | `a >= b`  |
| Signed Less Than          | `a @< b`  |
| Signed Greater Than       | `a @> b`  |
| Signed Less or Equal      | `a @<= b` |
| Signed Greater or Equal   | `a @>= b` |
| Bitwise And               | `a & b`   |
| Bitwise Or                | `a \| b`  |
| Bitwise Xor               | `a ^ b`   |
| Logical Shift Right       | `a >> b`  |
| Arithmetic Shift Right    | `a @>> b` |
| Shift Left                | `a << b`  |

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

#### Addition

`a + b` evaluates to the sum of `a` and `b`.

#### Subtraction

`a - b` evaluates to the difference between `a` and `b`.

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

#### Logical Shift Right

`a >> b` moves the bits of `a` `b` spaces over to the right, dropping the lowest `b` bits, and filling the highest `b` bits with 0.

#### Arithmetic Shift Right

`a >> b` moves the bits of `a` `b` spaces over to the right, dropping the lowest `b` bits, and filling the highest `b` bits with the highest bit of `a`.

#### Shift Left

`a << b` moves the bits of `a` `b` spaces over to the right, dropping the highest `b` bits, and filling the lowest `b` bits with 0.
