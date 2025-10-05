/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * A relatively simple loader program that sets up and runs the binaries
 * */

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef __GNUC__
#error "Requires GNU C extensions"
#endif
#ifndef __x86_64__
#error "Only works on x86_64"
#endif
#if __STDC_VERSION__ < 202311L
#error "C23 required"
#endif

[[gnu::sysv_abi]]
[[gnu::naked]]
static void run(void *vars, void *foreign_funcs, void *start) {
    asm(
        // save callee-saved registers
        "push %rbx\n"
        "push %rbp\n"
        // move the vars address into rbx
        "mov %rdi, %rbx\n"
        // move the foreign_funcs address into rsi
        "mov %rsi, %rbp\n"
        // jump to the start address
        "call *%rdx\n"
        // restore callee-saved registers
        "pop %rbx\n"
        "pop %rbp\n"
        // return
        "ret\n"
    );
}

int main(int argc, char *argv[]) {
    // SSIZE_MAX is defined, but OFF_MAX isn't, so make sure they're the same
    // size types so that SSIZE_MAX can be used for off_t overflow checks
    static_assert(sizeof(off_t) == sizeof(ssize_t));

    if (argc <= 1) {
        fputs("Error: No file provided\n", stderr);
        return EXIT_FAILURE;
    }

    for (int i = 2; i < argc; ++i) {
        fprintf(stderr, "Warning: ignoring nexpected argument: %s\n", argv[i]);
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (4096 % page_size != 0) {
        fprintf(
            stderr, "Error: system page size %ld is not supported.\n", page_size
        );
        return EXIT_FAILURE;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        return 1;
    }

    union {
        char bytes[32];

        struct [[gnu::packed]] [[gnu::aligned(8)]] {
            char magic[4];
            uint8_t version;
            uint8_t num_ffi_handles;
            uint16_t num_ffi_funcs;
            uint64_t ffi_size;
            uint64_t var_size;
            uint64_t code_size;
        };
    } header;

    ssize_t read_count = read(fd, header.bytes, 32);

    if (read_count != 32) {
        fputs("Failed to read header from file.\n", stderr);
        goto err_after_opening;
    }

    if (memcmp(header.magic, "\xf8UCF", 4) != 0) {
        fputs("Magic bytes mismatch.\n", stderr);
        goto err_after_opening;
    }

    if (header.version != 0) {
        fprintf(stderr, "Invalid version: %" PRIu8 "\n", header.version);
        fputs("(Expected version 0)\n", stderr);
        goto err_after_opening;
    }

    if (header.num_ffi_handles != 0) {
        fputs("FFI not yet implemented.\n", stderr);
        goto err_after_opening;
    }

    if (header.num_ffi_funcs != 0) {
        fputs("FFI not yet implemented.\n", stderr);
        goto err_after_opening;
    }

    if (header.ffi_size + 32 > SSIZE_MAX) {
        fputs("Variable offset too large.\n", stderr);
        goto err_after_opening;
    }

    off_t var_page_start = 32 + header.ffi_size;
    off_t var_offset = var_page_start & 0xfff;
    var_page_start &= ~0xfff;

    // make sure that there won't be alignment issues with variable start
    // address
    assert((var_offset & 0b111) == 0);

    void *variables = NULL;

    if (header.var_size) {
        variables = mmap(
            NULL,
            header.var_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE,
            fd,
            var_page_start
        );
        if (variables == MAP_FAILED) {
            perror("Failed to mmap variables segment");
            goto err_after_opening;
        }
        variables = ((char *)variables) + var_offset;
    }

    // TODO: FFI logic

    if (header.ffi_size != 0) {
        fputs("FFI not yet implemented.\n", stderr);
        goto err_after_opening;
    }

    if (header.ffi_size != 0) {
        fputs("Variable Segment not yet implemented.\n", stderr);
        goto err_after_opening;
    }

    if (header.code_size > SIZE_MAX) {
        fputs("Code segment too large to load\n", stderr);
        goto err_after_opening;
    }

    if (header.code_size == 0) {
        fputs("Code segment can't be empty.\n", stderr);
        goto err_after_opening;
    }

    void *foreign_funcs = NULL;

    size_t code_page_start = var_page_start + var_offset + header.var_size;
    if (code_page_start & 0xfff) {
        if (code_page_start > (SSIZE_MAX - 0x1000)) {
            fprintf(stderr, "Code offset %zu too large\n", code_page_start);
            goto err_after_opening;
        }
        code_page_start &= ~0xfff;
        code_page_start += 0x1000;
    }

    void *code = mmap(
        NULL,
        header.code_size,
        PROT_READ | PROT_EXEC,
        MAP_PRIVATE,
        fd,
        code_page_start
    );
    if (code == MAP_FAILED) {
        perror("Failed to mmap code segment");
        goto err_after_opening;
    }

    close(fd);

    run(variables, foreign_funcs, code);
    return EXIT_SUCCESS;

err_after_opening:
    close(fd);
    return EXIT_FAILURE;
}
