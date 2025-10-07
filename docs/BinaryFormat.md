<!--
SPDX-FileCopyrightText: 2025 Eli Array Minkoff

SPDX-License-Identifier: 0BSD
-->

# The UCF Binary Format and Loader

This document describes the "Untitled Custom Format" that this project is built around, and the loader program responsible for preparing the run environment for it on x86_64, and doing any required clean-up for that environment.

## Binary Format

The structure is very simple: there's a program header, an FFI segment, a variable segment, padding, then a code segment. That's it. Each segment's size can be determined from information within the program header. The code segment must be large enough to contain the process cleanup code, but other segments can be 0 bytes in size.

### Header

| Name            | C type     | Description                           |
|-----------------|------------|---------------------------------------|
| magic           | `char[4]`  | magic bytes identifying the file type |
| version         | `uint8_t`  | version number                        |
| num_ffi_handles | `uint8_t`  | number of FFI `dlopen` handles        |
| num_ffi_funcs   | `uint16_t` | number of FFI functions               |
| ffi_size        | `uint64_t` | Size of FFI segment                   |
| var_size        | `uint64_t` | Size of variable segment              |
| code_size       | `uint64_t` | Size of code segment                  |

The magic bytes will be `"\xf8UCF"`, with the following reasoning:
* `0xf8` cannot appear in valid UTF-8 text, marking it as a binary file
* `"UCF"` stands for "Untitled Custom Format"

All integer fields are stored with a little-endian byte order.

#### FFI Segment

The FFI segment format is to be determined. It must encode the information needed to properly `dlopen` and `dlsym` all required functions.

First, there is a series of up to 255 null-terminated filename strings, stored consecutively, and packed together - the number is specified by the `num_ffi_handles` field in the header.

Next, there are a series of up to 65,535 `FFIFunc`s, which have the following structure:

```c
typedef struct {
    // The index within the array of dynamic library handles
    uint8_t handle_index;
    // The NULL-terminated symbol to load from the handle
    char symbol[];
} FFIFunc;
```
* `symbol` is the symbol to load from the library with `dlsym`.



#### Variable Segment

This segment will be `mmap`ed as readable and writable, and is where all variables will be stored. The `mmap` may include the previous segments if needed for alignment reasons.

#### Code Segment

This segment will be `mmap`ed as readable and executable, and is where the machine code is stored. It will start with NUL bytes to pad the entry point to align the first instruction to an offset that's a multiple of 4096 bytes.

A minimum file would start with the following:

```xxd
00000000: f855 4346 0000 0000 0000 0000 0000 0000  .UCF............
00000010: 0000 0000 0000 0000 0100 0000 0000 0000  ................
```

Then, after 4064 padding bytes, it would have the following:
```xxd
00001000: c3                                       .
```

*(`c3` is the x86 `ret` instruction's encoding)*

## Loader

The loader program first parses and validates the header and runtime environment. If successful, it places a pointer to the array of foreign function addresses in the `RBP` register, and the address of the variable segment start in the `RBX` register, then passes control into the code segment using the x86 `call` instruction. If the validation or setup fail, it prints an error and returns exit code 1; If the code segment returns, it returns exit code 0, but the code segment itself might not return.

Registers other than `RBP` and `RBX` have unspecified starting values.
