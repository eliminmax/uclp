<!--
SPDX-FileCopyrightText: 2025 Eli Array Minkoff

SPDX-License-Identifier: 0BSD
-->

The binaries in this directory were made manually, using the [hed](https://github.com/fr0zn/hed) hex editor, as well as the [rasm2](https://book.rada.re/tools/rasm2/intro.html) assembler from the Radare2 toolkit, and some coreutils commands (`fallocate` to create the first one, and `cp` for all others.)

They were created while working out the structure of the binaries for this project and testing the loader program for them, and are as follows:

| Binary      | Behavior                                                        | Purpose                                                         |
|-------------|-----------------------------------------------------------------|-----------------------------------------------------------------|
| `mini`      | return to loader immediately                                    | Validate that loader properly runs the simplest possible binary |
| `false`     | invoke `exit` syscall                                           | Validate that system calls work properly                        |
| `ud2`       | execute an illegal instruction                                  | Check that machine code is executed directly                    |
| `hello`     | Print `"Hello, world!\n"` using the `write` syscall             | Check that reading variables works properly                     |
| `ffi_hello` | Print `"Hello, world!\n"` using the `puts` function from `libc` | Check that FFI calls work properly                              |
