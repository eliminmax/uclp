/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "mem/checked.h"
#include "mem/dynarray.h"
#include "safety.h"
#include "scanner.h"
#include "token.h"
#include "typenames.h"

#define BUF_SIZE 0x4000

static String read_source(const char *path) {
    int fd = open(path, O_RDONLY);
    int err;
    String s = (String){0, NULL};
    if (fd < 0) goto read_error;

    errno = 0;
    off_t file_size = lseek(fd, 0, SEEK_END);
    err = errno;
    REQUIRE(err != EBADF);
    lseek(fd, 0, SEEK_SET);

    switch (err) {
        [[clang::likely]] case 0: {
            ssize_t read_count;
            size_t remaining = file_size;
            s.len = file_size;
            char *_Nonnull source = checked_malloc(file_size);
            char *_Nonnull head = source;
            while ((read_count = read(fd, head, remaining)) != 0) {
                if (read_count < 0) [[clang::unlikely]]
                    goto read_error;
                head += read_count;
                remaining -= read_count;
            }
            s.text = source;
            break;
        }
        case EBADF:
            unreachable();
        case ESPIPE:
        case EINVAL:
        case EOVERFLOW: {
            ssize_t read_count;
            uchar *read_buf = checked_malloc(BUF_SIZE);
            CharBuffer buf = (CharBuffer){};
            while ((read_count = read(fd, read_buf, BUF_SIZE)) != 0) {
                if (read_count < 0) [[clang::unlikely]] { goto read_error; }
                append_chars(&buf, read_count, read_buf);
            }
            s.len = buf.size;
            s.text = checked_realloc(buf.mem, buf.size);
            free(read_buf);
        }
        [[clang::unlikely]] default: {
            fprintf(
                stderr,
                "Unexpected error when checking file size of `%s`: %s\n",
                path,
                strerror(err)
            );
            exit(2);
        }
    }
    return s;

read_error:
    err = errno;
    fprintf(
        stderr,
        "Failed to read source code from `%s`: %s\n",
        path,
        strerror(err)
    );
    exit(2);
}

static void debug_print_token(Token TOK) {
#define DEBUG_TOKENS
#include "include/lang/tokens.h"
}

int main(int argc, char *argv[]) {
    fprintf(stderr, "argc: %d\n", argc);
    if (argc < 2) {
        char *line = NULL;
        size_t len = 0;
        ssize_t sz;
        while ((sz = getline(&line, &len, stdin))) {
            if (sz == -1) {
                perror("Failed to read input");
                free(line);
                return 1;
            }
            len = sz;
            Scanner scanner = start_scanner((String){len, line});
            Token t;
            while ((t = next_token(scanner)).type != TOKEN_EOF) {
                debug_print_token(t);
                destroy_token(t);
            }
            free_scanner(scanner);
        }
        free(line);
        return 0;
    } else if (argc == 2) {
        bool had_error = false;
        String source = read_source(argv[1]);
        Scanner scanner = start_scanner(source);
        Token t;
        while ((t = next_token(scanner)).type != TOKEN_EOF) {
            if (t.type == TOKEN_UNPARSEABLE) had_error = true;
            debug_print_token(t);
            destroy_token(t);
        }
        free_scanner(scanner);
        free(source.text);
        return had_error;
    }
}

// the "home" of the normally-inlined small utility functions defined in
// various project headers

extern inline void destroy_token(Token);
