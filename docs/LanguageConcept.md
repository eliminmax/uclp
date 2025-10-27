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

Variables are declared with the following format:

```uclp
let $NAME: $TYPE;
let NAME: TYPE = DEFAULT_VALUE;
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
| Logical Shift Left        | `a << b`  |
| Arithmetic Shift Right    | `a @>> b` |
| Arithmetic Shift Left     | `a @<< b` |

