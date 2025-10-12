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
    * [Variable Declarations](#variable-declarations)
    * [Function Declarations](#function-declarations)
    * [Conditionals](#conditionals)
    * [While Loop](#while-loop)

<!-- vim-markdown-toc -->

UCLP is intended to be a limited language that's easy to compile, not a particularly robust or expressive language.

At time of writing this, the loader program for the binary format is working, but the language itself is just a vague idea. This document aims to be a place where I can narrow down the syntax and semantics.

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
