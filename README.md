<!--
SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff

SPDX-License-Identifier: 0BSD
-->

# UCLP - Untitled Custom Language Project

I have decided to create my own language that compiles to a custom binary executable format, just to have fun with Linux's `binfmt_misc`, and learn a bit more about computing.

I have very little idea what I'm doing, but my previous favorite projects have often started out that way, and have given me a little knowledge to start with - enough to be dangerous.

ALL INFORMATION IN THIS DOCUMENT IS PROVISIONAL AND SUBJECT TO CHANGE. THIS REPO CONTAINS A HOBBY PROJECT THAT SHOULD NOT BE USED FOR ANYTHING IMPORTANT!

## Goals and Non-Goals

### Language Goals

* [ ] Limited, easy to parse syntax
* [ ] Support for basic arithmetic, comparison, and bitwise operations on 8, 16, 32, and 64-bit integer types, using the semantics of x86_64 machine code.
* [ ] A minimal type system with support for signed and unsigned sized integer types, and fixed-size byte buffers.
* [ ] Support for `while` loops, as well as `if` and `if-else` statements.
* [ ] Compiles to a custom binary format, which contains machine code. Basic I/O can be done using the `read` and `write` Linux system calls on `stdin` and `stdout`, respectively.
* [ ] Limited FFI support - external functions can be declared, if they only have supported types. The architecture's SYSV ABI is used.
* [ ] Build an interpreter with debug tracing.
* [ ] Support for Linux system calls as functions

### Binary Format Goals

* [ ] Use a subset of `x86_64` machine code as the encoded instruction format.
* [x] Simple header format, declaring the following:
 * [x] Entry point
 * [x] Information needed for the loader or interpreter to properly call `dlopen` and `dlsym`
 * [x] Size of variable section
* [x] a loader program that can `mmap` the code and call it using inline assembly

### Other goals

* [ ] Neovim plugin for language support
* [ ] `binfmt_misc` support
* [ ] Look into process of setting up LSP support for the language.

### Non-goals

* Support for floating point types, classes, structs, or advanced type system trickery
* Built-in support for dynamic allocation
  * The use of the FFI system enables the use of `malloc` and its associated functions
* Efficient codegen
* Macro support
* Fast compile times
* Practical real-world usability
* Portability to non-Linux systems
* Portable dynamic linking
* Support for non-`amd64` architectures.

### To be determined

Ideas that may or may not become goals in the future
* File I/O (yes, everything is a file, but you know what I mean by this).
* Support for FFI on non-`glibc` Linux systems
* Debug info generation
