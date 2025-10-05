<!--
SPDX-FileCopyrightText: 2025 Eli Array Minkoff

SPDX-License-Identifier: 0BSD
-->

# Binary Format Description

The structure is very simple: there's a program header, an FFI segment, a variable segment, padding, then a code segment. That's it. Each segment's size can be determined from information within the program header. The code segment must be large enough to contain the process cleanup code, but other segments can be 0 bytes in size.

## Header

| C type     | Description                           |
|------------|---------------------------------------|
| `char[4]`  | magic bytes identifying the file type |
| `uint8_t`  | version number                        |
| `uint8_t`  | number of FFI `dlopen` handles        |
| `uint16_t` | number of FFI functions               |
| `uint64_t` | Size of FFI segment                   |
| `uint64_t` | Size of variable segment              |
| `uint64_t` | Size of code segment                  |

The magic bytes will be `"\xf8UCF"`, with the following reasoning:
* `0xf8` cannot appear in valid UTF-8 text, marking it as a binary file
* `"UCF"` stands for "Untitled Custom Format"

All integer fields are stored with a little-endian byte order.

### FFI Segment

The FFI segment format is to be determined. It must encode the information needed to properly `dlopen` and `dlsym` all required functions.

### Variable Segment

This segment will be `mmap`ed as readable and writable, and is where all variables will be stored. The `mmap` may include the previous segments if needed for alignment reasons.

### Code Segment

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

(`c3` is the x86 `ret` instruction's encoding)
