/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * A relatively simple loader program that sets up and runs the binaries
 * */

#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "platform_check.h"
#include "header_structs.h"

#if !__has_c_attribute(gnu::sysv_abi)
#error "Missing required GNU function attribute sysv_abi"
#endif

#if !__has_c_attribute(gnu::naked)
#error "Missing required GNU function attribute naked"
#endif

[[gnu::sysv_abi]] [[gnu::naked]]
static void run(
    const void *vars, const void *foreign_funcs, const void *start
) {
    __asm__(
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

// validate that the environment
static bool validate_environment(int argc, char *argv[]) {
    // POSIX requires this, but ISO C doesn't, so double-check
    static_assert(CHAR_BIT == 8, "8-bit char support required");

    // SSIZE_MAX is defined, but OFF_MAX isn't, so make sure they're the same
    // size types so that SSIZE_MAX can be used for off_t overflow checks
    static_assert(
        sizeof(off_t) == sizeof(ssize_t),
        "off_t must be the same size as ssize_t"
    );

    static_assert(sizeof(Header) == 32, "Header struct is the wrong size");

    if (argc <= 1) {
        fputs("Error: No file provided\n", stderr);
        return false;
    }

    for (int i = 2; i < argc; ++i) {
        fprintf(stderr, "Warning: ignoring unexpected argument: %s\n", argv[i]);
    }

    long page_size = sysconf(_SC_PAGESIZE);

    if (4096 % page_size != 0) {
        fprintf(
            stderr, "Error: system page size %ld is not supported.\n", page_size
        );
        return false;
    }
    return true;
}

static bool validate_header(const Header *header) {
#define CHECK(assertion, message, ...) \
    if (!(assertion)) { \
        fprintf(stderr, message __VA_OPT__(, ) __VA_ARGS__); \
        return false; \
    }

    CHECK(
        memcmp(header->magic, UCF_MAGIC, 4) == 0,
        "Magic bytes mismatch:\n"
        "\texpected: {f8 55 43 46}\n"
        "\tgot:      {%02x %02x %02x %02x}\n",
        (unsigned char)header->magic[0],
        (unsigned char)header->magic[1],
        (unsigned char)header->magic[2],
        (unsigned char)header->magic[3]
    );
    CHECK(
        header->version == UCF_VERSION_DEV,
        "Invalid version %" PRIu8 "\nSupported version: 0\n",
        header->version
    );
    CHECK(
        header->ffi_size + sizeof(Header) <= SSIZE_MAX,
        "Variable offset too large.\n"
    );
    CHECK((header->ffi_size & 0b111) == 0, "Unaligned variable segment\n");
    CHECK(header->var_size <= SIZE_MAX, "Variable segment too large to load\n");
    CHECK(header->code_size <= SIZE_MAX, "Code segment too large to load\n");
    CHECK(header->code_size != 0, "Code segment is empty\n");

    return true;
#undef CHECK
}

static void *FOREIGN_FUNCS[UINT16_MAX] = {};

static bool load_foreign_funcs(
    uint8_t nhandles, uint16_t nfuncs, const char *ffi_segment
) {
    void *handles[256] = {};
    const char *lib_names[256] = {};
    // pointer used to read the ffi segment.
    const char *reader = ffi_segment;
    // used for the dlopen's messy error checking and handling approach
    char *err;

#define NEXT_ITEM() \
    do { \
        while (*reader) ++reader; \
        ++reader; \
    } while (false)

    for (uint8_t i = 0; i < nhandles; ++i) {
        lib_names[i] = reader;
        handles[i] = dlopen(reader, RTLD_LAZY);

        if ((err = dlerror()) != NULL) {
            fprintf(
                stderr,
                "Failed to get handle for library \"%s\": %s.\n",
                reader,
                err
            );
            return false;
        }
        NEXT_ITEM();
    }

    for (uint16_t i = 0; i < nfuncs; ++i) {
        uint8_t handle_index = (uint8_t)*reader;
        ++reader;
        if (handle_index >= nhandles) {
            fprintf(
                stderr,
                "Symbol %s requires nonexistent handle #%" PRIu8 ".\n",
                reader,
                handle_index
            );
            return false;
        }

        // dlsym might return NULL on non-errors. The way to properly check it
        // is to see if dlerror returns a non-null value - in that case, an
        // error has occurred since the last call to dlerror.
        FOREIGN_FUNCS[i] = dlsym(handles[handle_index], reader);

        if ((err = dlerror())) {
            fprintf(
                stderr,
                "Failed to load symbol %s from %s: %s\n",
                reader,
                lib_names[handle_index],
                err
            );
            return false;
        }
        NEXT_ITEM();
    }

    return true;
}

int main(int argc, char *argv[]) {
    if (!validate_environment(argc, argv)) return EXIT_FAILURE;

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Failed to open file");
        return 1;
    }

    union {
        char bytes[sizeof(Header)];
        Header header;
    } header_reader;

    Header *header = &header_reader.header;

    ssize_t read_count = read(fd, header_reader.bytes, sizeof(Header));

    if (read_count != sizeof(Header)) {
        fputs("Failed to read header from file.\n", stderr);
        goto err_after_opening;
    }
    if (!validate_header(header)) goto err_after_opening;

    if (header->num_ffi_handles || header->num_ffi_handles) {
        // starting page also has header, so make sure to leave room
        size_t map_size = header->ffi_size + sizeof(Header);
        char *ffi_page = mmap(NULL, map_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (ffi_page == MAP_FAILED) {
            perror("Failed to mmap ffi segment");
            goto err_after_opening;
        }

        bool loaded_foreign_funcs = load_foreign_funcs(
            header->num_ffi_handles,
            header->num_ffi_funcs,
            // skip past the header to the start of the ffi segment
            &ffi_page[sizeof(Header)]
        );
        munmap(ffi_page, map_size);
        if (!loaded_foreign_funcs) goto err_after_opening;
    }

    off_t var_page_start = (sizeof(Header) + header->ffi_size) & ~0xfff;
    off_t var_offset = (sizeof(Header) + header->ffi_size) & 0xfff;

    void *variables = NULL;
    if (header->var_size) {
        variables = mmap(
            NULL,
            header->var_size,
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

    size_t code_page_start = var_page_start + var_offset + header->var_size;
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
        header->code_size,
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

    run(variables, FOREIGN_FUNCS, code);
    return EXIT_SUCCESS;

err_after_opening:
    close(fd);
    return EXIT_FAILURE;
}
