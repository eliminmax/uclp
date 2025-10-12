# Language Overview

<!-- vim-markdown-toc GFM -->

* [Type System](#type-system)
    * [Basic Integral Types](#basic-integral-types)
    * [Arrays](#arrays)
    * [Pointers](#pointers)
    * [External types](#external-types)
* [Data](#data)
* [Program Structure + Control Flow](#program-structure--control-flow)

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

## Data

Data is stored in the `variable` segment. There is concept of a stack or heap.

## Program Structure + Control Flow

Functions, while loops, and `if` statements all work more-or-less how they would in C.

`foreach` loops can iterate over all elements in arrays.
