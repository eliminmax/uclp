/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "mem/checked.h"
#include "mem/dynarray.h"
#include "parser.h"
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
        case EBADF: UNREACHABLE("fd was just opened");
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
    close(fd);
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

[[noreturn]] [[gnu::cold]] [[gnu::format(__printf__, 2, 5)]]
void report_ice(
    const char *name, const char *fmt, const char *file, int line, ...
) {
    fprintf(stderr, "INTERNAL COMPILER ERROR: %s (%s:%d)\n", name, file, line);
    va_list ap;
    va_start(ap, line);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(NULL);
    abort();
}

void debug_print_token(Token TOK) {
#define DEBUG_TOKENS
#include "include/lang/tokens.h"
}

struct line_alloc {
    char *_Nonnull text;
    size_t len;
    struct line_alloc *_Nullable prev;
};

struct line_alloc *_Nullable repl_line(struct line_alloc *_Nullable prev) {
    struct line_alloc *next_line = checked_malloc(sizeof(*next_line));
    size_t n = 0;
    char *text = NULL;
    errno = 0;
    ssize_t ret = getline(&text, &n, stdin);
    if (ret < 0) {
        free(text);
        if (errno) [[clang::unlikely]] {
            perror("Failed to read input");
            abort();
        }
        free(next_line);
        return NULL;
    }

    REQUIRE(text != NULL);
    next_line->text = text;

    next_line->len = ret;
    next_line->prev = prev;
    return next_line;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        struct line_alloc *line = NULL;
        struct line_alloc *new_line;
        while ((new_line = repl_line(line))) {
            line = new_line;
            Parser p = parser((String){line->len, line->text});
            Statement s = {.tag = STMT_UNPARSABLE};
            while ((s = get_statement(p)).tag != STMT_END) { dbg_stmt(p, &s); }
            free_parser(p);
        }

        while (line) {
            struct line_alloc *prev = line->prev;
            free(line->text);
            free(line);
            line = prev;
        }

        return 0;

    } else if (argc == 2) {
        AllocGroup ag = group_create();
        bool had_error = false;
        StringPool idents = new_pool(ag);
        String source = read_source(argv[1]);
        Scanner scanner = start_scanner(source, idents);
        Token t;
        while ((t = next_token(scanner)).type != TOKEN_EOF) {
            if (t.type == TOKEN_UNPARSEABLE) had_error = true;
            debug_print_token(t);
        }

        free_scanner(scanner);
        free(source.text);
        free_pool(idents);
        group_free(ag);
        return had_error;
    }
}
