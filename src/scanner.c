/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "colorize.h"
#include "mem/arena.h"
#include "mem/checked.h"
#include "mem/dynarray.h"
#include "safety.h"
#include "scanner.h"
#include "token.h"

#define BUF_SIZE 0x4000
#define MAX_ERR_TOKEN_SIZE 0x1000

#define NODEREF
#ifdef __has_attribute
#if __has_attribute(__noderef__)
#undef NODEREF
#define NODEREF __attribute__((__noderef__))
#endif
#endif

struct scanner {
    AllocGroup allocs;
    size_t source_size;
    uchar *source [[clang::counted_by(source_size)]];
    uchar *start;
    uchar *head;
    uchar *line_start;
    uchar NODEREF *end;
    size_t line;
    jmp_buf jump_buf;
} scanner;

void start_scanner(const char *path) {
    int fd = open(path, O_RDONLY);
    int err;
    if (fd < 0) goto read_error;

    scanner.allocs = group_create();
    scanner.line = 1;

    errno = 0;
    off_t file_size = lseek(fd, 0, SEEK_END);
    err = errno;
    REQUIRE(err != EBADF);
    lseek(fd, 0, SEEK_SET);

    switch (err) {
        [[clang::likely]] case 0: {
            ssize_t read_count;
            size_t remaining = file_size;
            scanner.source_size = file_size;
            uchar *_Nonnull source = checked_malloc(file_size);
            uchar *_Nonnull head = source;
            while ((read_count = read(fd, head, remaining)) != 0) {
                if (read_count < 0) [[clang::unlikely]] { goto read_error; }
                head += read_count;
                remaining -= read_count;
            }
            scanner.source = source;
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
            scanner.source_size = buf.size;
            scanner.source = checked_realloc(buf.mem, buf.size);
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
    scanner.line_start = scanner.start = scanner.head = scanner.source;
    scanner.end = &scanner.source[scanner.source_size];
    return;

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

void end_scanner() {
    free(scanner.source);
    group_free(scanner.allocs);
}

static int peek() {
    if (scanner.head + 1 < scanner.end) [[clang::likely]] {
        return scanner.head[1];
    }
    return -1;
}

static bool advance_on(char chr) {
    if (peek() == chr) {
        scanner.head++;
        return true;
    }
    return false;
}

static int advance() {
    if (scanner.head < scanner.end) {
        uchar c = *++scanner.head;
        if (c == '\n') {
            scanner.line++;
            scanner.line_start = scanner.head + 1;
        }
        return c;
    }
    return -1;
}

#define LITERAL(lit) ((String){.len = sizeof(lit) - 1, .text = lit})
#define DYNAMIC ((String){0, NULL})
// clang-format off

// Static Token lexemes to avoid too many small allocations
//
// If set to {0, NULL}, then it should be set dynamically.
//
// Definitions are orderered by common prefixes, with shorter strings after
// longer ones, to make it easier to follow when implementing the token matching
// logic.
static String TOKEN_STRINGS[TOKEN_EOF + 1] = {
    [TOKEN_NE]                = LITERAL("!="),
    [TOKEN_STR]               = DYNAMIC,
    [TOKEN_LOGICAL_NOT]       = LITERAL("!"),
    [TOKEN_POUND_SIGN]        = LITERAL("#"),
    [TOKEN_SIGNED_MOD_ASSIGN] = LITERAL("$%="),
    [TOKEN_SIGNED_MOD]        = LITERAL("$%"),
    [TOKEN_SIGNED_MUL_ASSIGN] = LITERAL("$*="),
    [TOKEN_SIGNED_MUL]        = LITERAL("$*"),
    [TOKEN_SIGNED_DIV_ASSIGN] = LITERAL("$/="),
    [TOKEN_SIGNED_DIV]        = LITERAL("$/"),
    [TOKEN_SIGNED_CAST]       = LITERAL("$:["),
    [TOKEN_SIGNED_LE]         = LITERAL("$<="),
    [TOKEN_SIGNED_LT]         = LITERAL("$<"),
    [TOKEN_SIGNED_GE]         = LITERAL("$>="),
    [TOKEN_SHR_ARITH_ASSIGN]  = LITERAL("$>>="),
    [TOKEN_SHR_ARITH]         = LITERAL("$>>"),
    [TOKEN_SIGNED_GT]         = LITERAL("$>"),
    [TOKEN_MOD_ASSIGN]        = LITERAL("%="),
    [TOKEN_MOD]               = LITERAL("%"),
    [TOKEN_LOGICAL_AND]       = LITERAL("&&"),
    [TOKEN_BIT_AND_ASSIGN]    = LITERAL("&="),
    [TOKEN_BIT_AND]           = LITERAL("&"),
    [TOKEN_CHAR]              = DYNAMIC,
    [TOKEN_L_PAREN]           = LITERAL("("),
    [TOKEN_R_PAREN]           = LITERAL(")"),
    [TOKEN_MUL_ASSIGN]        = LITERAL("*="),
    [TOKEN_MUL]               = LITERAL("*"),
    [TOKEN_PLUS_ASSIGN]       = LITERAL("+="),
    [TOKEN_PLUS]              = LITERAL("+"),
    [TOKEN_COMMA]             = LITERAL(","),
    [TOKEN_MINUS_ASSIGN]      = LITERAL("-="),
    [TOKEN_ARROW]             = LITERAL("->"),
    [TOKEN_MINUS]             = LITERAL("-"),
    [TOKEN_REF]               = LITERAL(".&"),
    [TOKEN_DEREF]             = LITERAL(".*"),
    [TOKEN_DIV_ASSIGN]        = LITERAL("/="),
    [TOKEN_DIV]               = LITERAL("/"),
    [TOKEN_UNSIGNED_CAST]     = LITERAL(":["),
    [TOKEN_COLON]             = LITERAL(":"),
    [TOKEN_SEMICOLON]         = LITERAL(";"),
    [TOKEN_SHL_ASSIGN]        = LITERAL("<<="),
    [TOKEN_SHL]               = LITERAL("<<"),
    [TOKEN_LE]                = LITERAL("<="),
    [TOKEN_LT]                = LITERAL("<"),
    [TOKEN_EQ]                = LITERAL("=="),
    [TOKEN_ASSIGN]            = LITERAL("="),
    [TOKEN_GE]                = LITERAL(">="),
    [TOKEN_SHR_LOG_ASSIGN]    = LITERAL(">>="),
    [TOKEN_SHR_LOG]           = LITERAL(">>"),
    [TOKEN_GT]                = LITERAL(">"),
    [TOKEN_L_SQUARE]          = LITERAL("["),
    [TOKEN_R_SQUARE]          = LITERAL("]"),
    [TOKEN_BIT_XOR_ASSIGN]    = LITERAL("^="),
    [TOKEN_BIT_XOR]           = LITERAL("^"),
    [TOKEN_L_CURLY]           = LITERAL("{"),
    [TOKEN_BIT_OR_ASSIGN]     = LITERAL("|="),
    [TOKEN_LOGICAL_OR]        = LITERAL("||"),
    [TOKEN_BIT_OR]            = LITERAL("|"),
    [TOKEN_R_CURLY]           = LITERAL("}"),
    // keywords last
    [TOKEN_DYNAMIC]           = LITERAL("dynamic"),
    [TOKEN_ELIF]              = LITERAL("elif"),
    [TOKEN_ELSE]              = LITERAL("else"),
    [TOKEN_FOREACH]           = LITERAL("foreach"),
    [TOKEN_FUNC]              = LITERAL("func"),
    [TOKEN_I16]               = LITERAL("i16"),
    [TOKEN_I32]               = LITERAL("i32"),
    [TOKEN_I64]               = LITERAL("i64"),
    [TOKEN_I8]                = LITERAL("i8"),
    [TOKEN_IF]                = LITERAL("if"),
    [TOKEN_IN]                = LITERAL("in"),
    [TOKEN_LET]               = LITERAL("let"),
    [TOKEN_LINUX]             = LITERAL("linux"),
    [TOKEN_OPAQUE]            = LITERAL("opaque"),
    [TOKEN_STATIC]            = LITERAL("static"),
    [TOKEN_WHILE]             = LITERAL("while"),
    // leave STR, CHAR, INT_*, IDENT, and UNPARSEABLE as {0, NULL}
    [TOKEN_INT_BIN]           = DYNAMIC,
    [TOKEN_INT_OCT]           = DYNAMIC,
    [TOKEN_INT_DEC]           = DYNAMIC,
    [TOKEN_INT_HEX]           = DYNAMIC,
    [TOKEN_UNPARSEABLE]       = DYNAMIC,
    [TOKEN_EOF]               = LITERAL(""),
};
// clang-format on
#undef DYNAMIC
#undef LITERAL

static void skip_line_comment() {
    while (scanner.head < scanner.end && *scanner.head != '\n') { advance(); }
}

static void skip_block_comment() {
    while (scanner.head < scanner.end) {
        switch (*scanner.head) {
            case '\n':
                scanner.line_start = ++scanner.head;
                scanner.line++;
                break;
            case '*':
                if (peek() == '/') {
                    scanner.head += 2;
                    return;
                }
                [[fallthrough]];
            default:
                scanner.head++;
        }
    }
}

// skip past whitespace and commas
static void skip_whitespace() {
    while (scanner.head < scanner.end) {
        switch (*scanner.head) {
            case '\n':
                scanner.line_start = ++scanner.head;
                scanner.line++;
                break;
            case '\r':
            case ' ':
            case '\f': // form feed character
            case '\v': // vertical tab
            case '\t':
                scanner.head++;
                break;
            case '/':
                if (advance_on('/')) {
                    skip_line_comment();
                    break;
                }
                if (advance_on('*')) {
                    skip_block_comment();
                    break;
                }
                [[fallthrough]];
            default:
                return;
        }
    }
}

static enum token_type signed_op() {
    if (++scanner.head == scanner.end) [[clang::unlikely]] {
        return TOKEN_UNPARSEABLE;
    }

    switch (*scanner.head) {
        case '%':
            if (advance_on('=')) return TOKEN_SIGNED_MOD_ASSIGN;
            return TOKEN_SIGNED_MOD;
        case '*':
            if (advance_on('=')) return TOKEN_SIGNED_MUL_ASSIGN;
            return TOKEN_SIGNED_MUL;
        case '/':
            if (advance_on('=')) return TOKEN_SIGNED_DIV_ASSIGN;
            return TOKEN_SIGNED_DIV;
        case ':':
            if (advance_on('[')) return TOKEN_SIGNED_CAST;
            return TOKEN_UNPARSEABLE;
        case '<':
            if (advance_on('=')) return TOKEN_SIGNED_LE;
            return TOKEN_SIGNED_LT;
        case '>':
            if (advance_on('=')) return TOKEN_SIGNED_GE;
            if (advance_on('>')) {
                if (advance_on('=')) return TOKEN_SHR_ARITH_ASSIGN;
                return TOKEN_SHR_ARITH;
            }
            return TOKEN_SIGNED_GT;
        default:
            return TOKEN_UNPARSEABLE;
    }
}

static String get_lexeme() {
    ptrdiff_t len = scanner.head - scanner.start;
    char *text = NULL;
    if (len) {
        void *_Nonnull mem = checked_malloc(len);
        text = memcpy(mem, scanner.start, len);
    }
    return (String){.len = len, .text = text};
}

// Don't use locale-sensitive <ctypes.h> functions for checks that are part of
// the language's syntax, but for
[[gnu::const]]
static bool is_hex_digit(uchar c) {
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
           (c >= '0' && c <= '9');
}

[[gnu::const]]
static bool is_ident_start(uchar c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

[[gnu::const]]
static bool is_ident_chr(uchar c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || (c == '_');
}

[[gnu::cold, gnu::format(__printf__, 1, 2), noreturn]]
static void scan_error(const char *_Nonnull fmt, ...) {
    REQUIRE(scanner.start < scanner.end);
    REQUIRE(*scanner.start < 0x80);

    uchar *line_end = scanner.head;
    while (line_end < scanner.end && *line_end != '\n') line_end++;

    ptrdiff_t prefix_len = scanner.start - scanner.line_start;
    ptrdiff_t token_len = scanner.head - scanner.start;
    ptrdiff_t suffix_len = line_end - scanner.head;

    va_list fmt_args;
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);

    if (token_len > MAX_ERR_TOKEN_SIZE || prefix_len > MAX_ERR_TOKEN_SIZE ||
        suffix_len > MAX_ERR_TOKEN_SIZE) {
        fprintf(stderr, "  (line %zu)", scanner.line);
        longjmp(scanner.jump_buf, 1);
    }

    fprintf(stderr, "(line %zu):\n", scanner.line);
    // dim italics
    write_color(scanner.line_start, stderr, prefix_len, "2;3");
    // bold red
    write_color(scanner.start, stderr, token_len, "1;31");
    write_color(scanner.head, stderr, suffix_len, "2; 3");
    longjmp(scanner.jump_buf, 1);
}

typedef struct {
    uchar text[5];
} EscapedChr;

[[gnu::const]]
static EscapedChr esc_chr(uchar c) {
    EscapedChr buf = {};
    switch (c) {
        case '\'':
        case '\\':
        case '\"':
            buf.text[0] = '\\';
            buf.text[1] = c;
            break;

        case '\n':
            buf.text[0] = '\\';
            buf.text[1] = 'n';
            break;
        case '\t':
            buf.text[0] = '\\';
            buf.text[1] = 't';
            break;
        case '\r':
            buf.text[0] = '\\';
            buf.text[1] = 'r';
            break;

        default:
            if (isprint(c)) {
                buf.text[0] = c;
            } else {
                sprintf((char *)buf.text, "\\x%02x", c);
            }
    }
    return buf;
}

static void scan_escape_char(const char *literal_type) {
    int chr;
    int next;
    uchar *esc_start = scanner.head;
    switch (chr = advance()) {
        case -1:
            goto eof;
        case '\n':
            scan_error("End of file in %s literal", literal_type);
        case '\'':
        case '\"':
        case '\\':
        case 'n':
        case 'r':
        case 't':
            return;
        case 'x':
            for (int i = 0; i < 2; i++) {
                if ((chr = advance()) == -1 || !is_hex_digit(chr)) {
                    if (chr == -1) { goto eof; }
                    scanner.start = esc_start;
                    scan_error(
                        "Expected hexadecimal digit in \\x escape, but got "
                        "'%s'",
                        esc_chr(chr).text
                    );
                }
            }
        case '0':
        case '1':
        case '2':
        case '3':
            // If the next digit is valid octal, advance then fall through to
            // the '4' ... '7' handling to handle the potential for a 3-digit
            // octal escape.
            //
            // If EOF, then
            if ((next = peek()) == -1) goto eof;

            if (next < '0' || next > '7') return;
            // advance and fall through to handle possible last digit
            advance();
            [[fallthrough]];
        case '4':
        case '5':
        case '6':
        case '7':
            // advance if the next digit is valid octal
            next = peek();
            if (next >= '0' && next <= '7') advance();
            return;
        default:
            scanner.start = esc_start;
            scan_error("Invalid escape sequence: '\\%s'", esc_chr(chr).text);
    }
eof:
    scan_error("End of file in %s literal", literal_type);
}

static enum token_type scan_string_literal() {
    advance();

    while (!advance_on('"')) {
        if (scanner.head == scanner.end) { break; }
    }

    __builtin_trap();
}

static enum token_type scan_char_literal() {
    int chr;
    switch ((chr = advance())) {
        case -1:
            scan_error("End of file in character literal");
        case '\'':
            if (advance_on('\'')) {
                scan_error("''' is not a valid character literal.");
            }
            scan_error("Empty character literal.");
        case '\\':
            scan_escape_char("character");
            break;
        case '\n':
            scan_error("End of line in character literal");
        default:
            break;
    }
    if (!advance_on('\'')) { scan_error("Missing end of character literal."); }
    return TOKEN_CHAR;
}

static void scan_numeric_literal() {
    __builtin_trap();
}

static enum token_type next_token_type() {
    skip_whitespace();

    if (scanner.head == scanner.end) return TOKEN_EOF;

    scanner.start = scanner.head;

    switch (*scanner.head) {
        case '!':
            if (advance_on('=')) return TOKEN_NE;
            return TOKEN_LOGICAL_NOT;
        case '"':
            return scan_string_literal();
        case '#':
            return TOKEN_POUND_SIGN;
        case '$':
            return signed_op();
        case '%':
            if (advance_on('=')) return TOKEN_MOD_ASSIGN;
            return TOKEN_MOD;
        case '&':
            if (advance_on('&')) return TOKEN_LOGICAL_AND;
            if (advance_on('=')) return TOKEN_BIT_AND_ASSIGN;
            return TOKEN_BIT_AND;
        case '\'':
            return scan_char_literal();
        case '(':
            return TOKEN_L_PAREN;
        case ')':
            return TOKEN_R_PAREN;
        case '*':
            if (advance_on('=')) return TOKEN_MUL_ASSIGN;
            return TOKEN_MUL;
        case '+':
            if (advance_on('=')) return TOKEN_PLUS_ASSIGN;
            return TOKEN_PLUS;
        case ',':
            return TOKEN_COMMA;
        case '-':
            if (advance_on('=')) return TOKEN_MINUS_ASSIGN;
            if (advance_on('>')) return TOKEN_ARROW;
            return TOKEN_MINUS;
        case '.':
            if (advance_on('&')) return TOKEN_REF;
            if (advance_on('*')) return TOKEN_DEREF;
            return TOKEN_UNPARSEABLE;
        case '/':
            if (advance_on('=')) return TOKEN_DIV_ASSIGN;
            return TOKEN_DIV;
        case ':':
            if (advance_on('[')) return TOKEN_UNSIGNED_CAST;
            return TOKEN_COLON;
        case ';':
            return TOKEN_SEMICOLON;
        case '<':
            if (advance_on('<')) {
                if (advance_on('=')) return TOKEN_SHL_ASSIGN;
                return TOKEN_SHL;
            }
            if (advance_on('=')) return TOKEN_LE;
            return TOKEN_LT;
        case '=':
            if (advance_on('=')) return TOKEN_EQ;
            return TOKEN_ASSIGN;
        case '>':
            if (advance_on('>')) {
                if (advance_on('=')) return TOKEN_SHR_LOG_ASSIGN;
                return TOKEN_SHR_LOG;
            }
            if (advance_on('=')) return TOKEN_GE;
            return TOKEN_GT;
        case '[':
            return TOKEN_L_SQUARE;
        case ']':
            return TOKEN_R_SQUARE;
        case '^':
            if (advance_on('=')) return TOKEN_BIT_XOR_ASSIGN;
            return TOKEN_BIT_XOR;
        case '{':
            return TOKEN_L_CURLY;
        case '|':
            if (advance_on('=')) return TOKEN_BIT_OR_ASSIGN;
            if (advance_on('|')) return TOKEN_LOGICAL_OR;
            return TOKEN_BIT_OR;
        case '}':
            return TOKEN_R_CURLY;
    }

    __builtin_trap();
}

Token next_token() {
    if (setjmp(scanner.jump_buf)) {
        advance();
        return (Token){
            .type = TOKEN_UNPARSEABLE,
            .line = scanner.line,
            .lexeme = get_lexeme(),
            .should_free_text = true,
        };
    }

    enum token_type type = next_token_type();

    size_t len = scanner.head - scanner.start;

    String lexeme;
    bool should_free_text = false;

    if ((lexeme = TOKEN_STRINGS[type]).text) {
        assert(len == lexeme.len);
        assert(memcmp(lexeme.text, scanner.head, len) == 0);
    } else {
        if (len) {
            should_free_text = true;
            lexeme = get_lexeme();
        } else {
            lexeme = (String){0, NULL};
        }
    }

    advance();

    return (Token){
        .type = type,
        .line = scanner.line,
        .lexeme = lexeme,
        .should_free_text = should_free_text,
    };
}
