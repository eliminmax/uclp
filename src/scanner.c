/*
 * SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "annotations.h"
#include "mem/checked.h"
#include "printerr.h"
#include "safety.h"
#include "scanner.h"
#include "token.h"

#define MAX_ERR_TOKEN_SIZE 0x1000

#define USE [[clang::use_handle("scanner")]]

struct scanner {
    size_t source_size;
    const uchar *source [[clang::counted_by(source_size)]];
    const uchar *start;
    const uchar *head;
    const uchar *line_start;
    const uchar NODEREF *end;
    size_t line;
    jmp_buf jump_buf;
    StringPool ident_pool;
};

[[clang::acquire_handle("scanner")]]
Scanner start_scanner(
    String source, [[clang::use_handle("pool")]] StringPool ident_pool
) {
    Scanner s = checked_malloc(sizeof(struct scanner));
    s->line = 1;
    s->source_size = source.len;
    s->line_start = s->start = s->head = s->source = (uchar *)source.text;
    s->end = &s->source[s->source_size];
    s->ident_pool = ident_pool;
    return s;
}

[[gnu::cold, gnu::format(__printf__, 2, 3), noreturn]]
static void scan_error(USE Scanner s, const char *_Nonnull fmt, ...) {
    REQUIRE(s->start < s->end);
    va_list fmt_args;
    va_start(fmt_args, fmt);

    ucl_verror(
        s->line,
        s->source,
        s->source_size,
        s->start,
        s->end - s->start,
        fmt,
        fmt_args
    );
    va_end(fmt_args);

    longjmp(s->jump_buf, 1);
}

const uchar *_Nonnull get_position([[clang::use_handle("scanner")]] Scanner s) {
    return s->head;
}

String get_source([[clang::use_handle("scanner")]] Scanner s) {
    return (String){.len = s->source_size, .text = (char*)s->source };
}

static int peek(USE Scanner s) {
    if (s->head + 1 < s->end) [[clang::likely]]
        return s->head[0];
    return -1;
}

static int peek2(USE Scanner s) {
    if (s->head + 2 < s->end) [[clang::likely]]
        return s->head[1];
    return -1;
}

static int advance(USE Scanner s) {
    if (s->head < s->end) {
        uchar c = *s->head++;
        if (c == '\n') {
            s->line++;
            s->line_start = s->head;
        }
        return c;
    }
    return -1;
}

static bool advance_on(char chr, USE Scanner s) {
    if (peek(s) == chr) {
        advance(s);
        return true;
    }
    return false;
}

static void skip_block_comment(USE Scanner s) {
    for (int chr = advance(s); chr != -1; chr = advance(s)) {
        if (chr == '*' && advance_on('/', s)) return;
    }
    scan_error(s, "End of file in block comment");
}

// skip past whitespace and commas
static void skip_whitespace(USE Scanner s) {
    while (s->head < s->end) {
        switch (*s->head) {
            case '\n':
            case '\r':
            case ' ':
            case '\t': advance(s); break;
            case '/':
                advance(s);
                if (advance_on('/', s)) {
                    for (int peeked = advance(s);
                         peeked != -1 && peeked != '\n';
                         peeked = advance(s));

                    break;
                } else if (advance_on('*', s)) {
                    skip_block_comment(s);
                    break;
                } else {
                    s->head--;
                    return;
                }
            default: return;
        }
    }
}

static String get_lexeme(USE Scanner s) {
    ptrdiff_t len = s->head - s->start;
    return (String){.len = len, .text = (char *)s->start};
}

// Don't use locale-sensitive <ctype.h> functions for checks that should not be
// locale-dependent

[[gnu::const]] [[gnu::artificial]]
static inline bool is_in_range(int c, int start, int end) {
    return c >= start && c <= end;
}

#define is_bin_digit(c) is_in_range((c), '0', '1')
#define is_oct_digit(c) is_in_range((c), '0', '7')
#define is_dec_digit(c) is_in_range((c), '0', '9')

[[gnu::const]]
static bool is_hex_digit(int c) {
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
           (c >= '0' && c <= '9');
}

[[gnu::const]]
static bool is_ident_start(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

[[gnu::const]]
static bool is_ident_chr(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || (c == '_');
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

static void scan_escape_char(
    const char *literal_type, uchar end, USE Scanner s
) {
    int chr;
    int next;
    const uchar *esc_start = s->head;
    switch (chr = advance(s)) {
        case -1: goto eof;
        case '\n': scan_error(s, "End of file in %s literal", literal_type);
        case '\'':
        case '\"':
        case '\\':
        case 'n':
        case 'r':
        case 't': return;
        case 'x':
            for (int i = 0; i < 2; i++) {
                if ((chr = advance(s)) == -1 || !is_hex_digit(chr)) {
                    if (chr == -1) goto eof;
                    s->start = esc_start;
                    scan_error(
                        s,
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
            if ((next = peek(s)) == -1) goto eof;

            if (next < '0' || next > '7') return;
            // advance and fall through to handle possible last digit
            advance(s);
            [[fallthrough]];
        case '4':
        case '5':
        case '6':
        case '7':
            // advance if the next digit is valid octal
            next = peek(s);
            if (next >= '0' && next <= '7') advance(s);
            return;
        default:
            s->start = esc_start;
            while (s->head < s->end && *s->head != end) advance(s);
            scan_error(s, "Invalid escape sequence: '\\%s'", esc_chr(chr).text);
    }
eof:
    scan_error(s, "End of file in %s literal", literal_type);
}

static void scan_string_literal(USE Scanner s) {
    while (!advance_on('"', s)) {
        switch (peek(s)) {
            case -1: scan_error(s, "End of file in string literal");
            case '\\':
                s->head++;
                scan_escape_char("string", '"', s);
                break;
            case '\n': scan_error(s, "End of line in string literal");
            case '\"':
                UNREACHABLE(
                    "advance_on('\"') would have ended the string at loop start"
                );
            default: s->head++;
        }
    }
}

static void scan_char_literal(USE Scanner s) {
    switch (peek(s)) {
        case -1: scan_error(s, "End of file in character literal");
        case '\'':
            s->head++;
            if (advance_on('\'', s)) {
                scan_error(s, "''' is not a valid character literal");
            }
            scan_error(s, "Empty character literal");
        case '\\':
            s->head++;
            scan_escape_char("character", '\'', s);
            break;
        case '\n': scan_error(s, "End of line in character literal");
        default: s->head++;
    }
    if (!advance_on('\'', s)) {
        scan_error(s, "Missing end of character literal");
    }
}

static enum token_type scan_size(
    enum token_type base, const char *kind, USE Scanner s
) {
    if (advance_on('8', s)) return base;
    if (advance_on('1', s)) {
        if (advance_on('6', s)) return base + 1;
    }
    if (advance_on('3', s)) {
        if (advance_on('2', s)) return base + 2;
    }
    if (advance_on('6', s)) {
        if (advance_on('4', s)) return base + 3;
    }
    while (is_dec_digit(peek(s))) advance(s);
    scan_error(s, "Invalid %s", kind);
}

static enum token_type scan_bin(USE Scanner s) {
    advance(s);
    if (!is_bin_digit(advance(s))) {
        scan_error(s, "Expect at least 1 digit after prefix");
    }
    while (true) {
        if (is_bin_digit(peek(s))) {
            s->head += 1;
        } else if (peek(s) == '_' && is_bin_digit(peek2(s))) {
            s->head += 2;
        } else {
            if (!advance_on('#', s)) return TOKEN_INT_BIN;
            return scan_size(TOKEN_INT_BIN_8, "size suffix", s);
        }
    }
}

static enum token_type scan_oct(USE Scanner s) {
    advance(s);
    if (!is_oct_digit(advance(s))) {
        scan_error(s, "Expect at least 1 digit after prefix");
    }
    while (true) {
        if (is_oct_digit(peek(s))) {
            s->head += 1;
        } else if (peek(s) == '_' && is_oct_digit(peek2(s))) {
            s->head += 2;
        } else {
            if (!advance_on('#', s)) return TOKEN_INT_OCT;
            return scan_size(TOKEN_INT_OCT_8, "size suffix", s);
        }
    }
}

static enum token_type scan_dec(USE Scanner s) {
    while (true) {
        if (is_dec_digit(peek(s))) {
            s->head += 1;
        } else if (peek(s) == '_' && is_dec_digit(peek2(s))) {
            s->head += 2;
        } else {
            if (!advance_on('#', s)) return TOKEN_INT_DEC;
            return scan_size(TOKEN_INT_DEC_8, "size suffix", s);
        }
    }
}

static enum token_type scan_hex(USE Scanner s) {
    advance(s);
    if (!is_hex_digit(advance(s))) {
        scan_error(s, "Expect at least 1 digit after prefix");
    }
    while (true) {
        if (is_hex_digit(peek(s))) {
            s->head += 1;
        } else if (peek(s) == '_' && is_hex_digit(peek2(s))) {
            s->head += 2;
        } else {
            if (!advance_on('#', s)) return TOKEN_INT_HEX;
            return scan_size(TOKEN_INT_HEX_8, "size suffix", s);
        }
    }
}

static enum token_type scan_numeric(USE Scanner s) {
    REQUIRE(s->head[-1] >= '0' && s->head[-1] <= '9');
    if (s->head[-1] == '0') {
        switch (peek(s)) {
            case 'b': return scan_bin(s);
            case 'o': return scan_oct(s);
            case 'x': return scan_hex(s);
        }
    }
    return scan_dec(s);
}

static enum token_type finish_cast(enum token_type base, USE Scanner s) {
    enum token_type type = scan_size(base, "bit width", s);
    if (!advance_on(']', s)) scan_error(s, "Missing end of size cast");
    return type;
}

static enum token_type scan_opaque(USE Scanner s) {
    if (!advance_on('.', s)) scan_error(s, "Expect '.' in opaque type");
    if (!is_dec_digit(peek(s))) scan_error(s, "Expect size in opaque type");
    s->head++;
    scan_numeric(s);
    if (!advance_on('.', s)) scan_error(s, "Expect '.' in opaque type");
    return scan_size(TOKEN_OPAQUE_8, "alignment", s);
}

static void scan_syscall(USE Scanner s) {
    if (!advance_on('.', s)) scan_error(s, "Expect '.' in syscall");
    if (!is_ident_start(peek(s))) scan_error(s, "Expect syscall name");
    s->head++;
    while (is_ident_chr(peek(s))) s->head++;
}

static enum token_type signed_op(USE Scanner s) {
    if (++s->head == s->end) [[clang::unlikely]] { return TOKEN_UNPARSEABLE; }

    switch (*s->head) {
        case '%':
            if (advance_on('=', s)) return TOKEN_SIGNED_MOD_ASSIGN;
            return TOKEN_SIGNED_MOD;
        case '*':
            if (advance_on('=', s)) return TOKEN_SIGNED_MUL_ASSIGN;
            return TOKEN_SIGNED_MUL;
        case '/':
            if (advance_on('=', s)) return TOKEN_SIGNED_DIV_ASSIGN;
            return TOKEN_SIGNED_DIV;
        case ':':
            if (advance_on('[', s)) {
                return finish_cast(TOKEN_SIGNED_BITCAST_8, s);
            }
            return TOKEN_UNPARSEABLE;
        case '<':
            if (advance_on('=', s)) return TOKEN_SIGNED_LE;
            return TOKEN_SIGNED_LT;
        case '>':
            if (advance_on('=', s)) return TOKEN_SIGNED_GE;
            if (advance_on('>', s)) {
                if (advance_on('=', s)) return TOKEN_SHR_ARITH_ASSIGN;
                return TOKEN_SHR_ARITH;
            }
            return TOKEN_SIGNED_GT;
        default: return TOKEN_UNPARSEABLE;
    }
}

// SPDX-SnippetCopyrightText: 2026 Eli Array Minkoff
// SPDX-SnippetCopyrightText: 2015 Robert Nystrom
//
// SPDX-License-Identifier: MIT

// A port of `checkKeyword` from clox (in Nystrom's Crafting Interpreters)
static enum token_type check_keyword(
    int start, int len, const char *rest, enum token_type type, USE Scanner s
) {
    if (s->head - s->start == start + len &&
        memcmp(s->start + start, rest, len) == 0) {
        return type;
    }

    return TOKEN_IDENT;
}

// includes a port of `identifierType` from Nystrom's Crafting Interpreters
static enum token_type scan_word(USE Scanner s) {
    ptrdiff_t sz;
    while (is_ident_chr(peek(s))) advance(s);
    switch (*s->start) {
        case 'd': return check_keyword(1, 6, "ynamic", TOKEN_DYNAMIC, s);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE, s); break;
        case 'f':
            if (s->head - s->start > 1) {
                switch (s->start[1]) {
                    case 'o':
                        return check_keyword(1, 6, "oreach", TOKEN_FOREACH, s);
                    case 'u': return check_keyword(1, 3, "unc", TOKEN_FUNC, s);
                }
            }
            break;
        case 'i':
            if ((sz = s->head - s->start) > 1) {
                switch (s->start[1]) {
                    case '1': return check_keyword(2, 1, "6", TOKEN_I16, s);
                    case '3': return check_keyword(2, 1, "2", TOKEN_I32, s);
                    case '6': return check_keyword(2, 1, "4", TOKEN_I64, s);
                    case '8': return sz == 2 ? TOKEN_I8 : TOKEN_IDENT;
                    case 'f': return sz == 2 ? TOKEN_IF : TOKEN_IDENT;
                    case 'n': return sz == 2 ? TOKEN_IN : TOKEN_IDENT;
                }
            }
            break;
        case 'l':
            if (s->head - s->start > 1) {
                return check_keyword(1, 2, "et", TOKEN_LET, s);
            }
            break;
        case 'o':
            if (s->head - s->start == 6 && memcmp(s->start, "opaque", 6) == 0) {
                return scan_opaque(s);
            }
            break;
        case 's':
            if (s->head - s->start == 3 && memcmp(s->start, "sys", 3) == 0) {
                scan_syscall(s);
                return TOKEN_SYS;
            }
            return check_keyword(1, 5, "tatic", TOKEN_STATIC, s);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE, s);
    }
    return TOKEN_IDENT;
}

// SPDX-SnippetEnd

static enum token_type next_token_type(USE Scanner s) {
    skip_whitespace(s);

    if (s->head == s->end) return TOKEN_EOF;

    s->start = s->head;

    switch (advance(s)) {
        case '!':
            if (advance_on('=', s)) return TOKEN_NE;
            return TOKEN_LOGICAL_NOT;
        case '"': scan_string_literal(s); return TOKEN_STR;
        case '#': return TOKEN_POUND_SIGN;
        case '$': return signed_op(s);
        case '%':
            if (advance_on('=', s)) return TOKEN_MOD_ASSIGN;
            return TOKEN_MOD;
        case '&':
            if (advance_on('&', s)) return TOKEN_LOGICAL_AND;
            if (advance_on('=', s)) return TOKEN_BIT_AND_ASSIGN;
            return TOKEN_BIT_AND;
        case '\'': scan_char_literal(s); return TOKEN_CHAR;
        case '(': return TOKEN_L_PAREN;
        case ')': return TOKEN_R_PAREN;
        case '*':
            if (advance_on('=', s)) return TOKEN_MUL_ASSIGN;
            return TOKEN_MUL;
        case '+':
            if (advance_on('=', s)) return TOKEN_PLUS_ASSIGN;
            return TOKEN_PLUS;
        case ',': return TOKEN_COMMA;
        case '-':
            if (advance_on('=', s)) return TOKEN_MINUS_ASSIGN;
            if (advance_on('>', s)) return TOKEN_ARROW;
            return TOKEN_MINUS;
        case '.':
            if (advance_on('&', s)) return TOKEN_REF;
            if (advance_on('*', s)) return TOKEN_DEREF;
            return TOKEN_DOT;
        case '/':
            if (advance_on('=', s)) return TOKEN_DIV_ASSIGN;
            return TOKEN_DIV;
        case ':':
            if (advance_on('[', s)) {
                return finish_cast(TOKEN_UNSIGNED_BITCAST_8, s);
            }
            if (advance_on('{', s)) return TOKEN_TYPE_CAST;
            return TOKEN_COLON;
        case ';': return TOKEN_SEMICOLON;
        case '<':
            if (advance_on('<', s)) {
                if (advance_on('=', s)) return TOKEN_SHL_ASSIGN;
                return TOKEN_SHL;
            }
            if (advance_on('=', s)) return TOKEN_LE;
            return TOKEN_LT;
        case '=':
            if (advance_on('=', s)) return TOKEN_EQ;
            return TOKEN_ASSIGN;
        case '>':
            if (advance_on('>', s)) {
                if (advance_on('=', s)) return TOKEN_SHR_LOG_ASSIGN;
                return TOKEN_SHR_LOG;
            }
            if (advance_on('=', s)) return TOKEN_GE;
            return TOKEN_GT;
        case '[': return TOKEN_L_SQUARE;
        case ']': return TOKEN_R_SQUARE;
        case '^':
            if (advance_on('=', s)) return TOKEN_BIT_XOR_ASSIGN;
            return TOKEN_BIT_XOR;
        case '{': return TOKEN_L_CURLY;
        case '|':
            if (advance_on('=', s)) return TOKEN_BIT_OR_ASSIGN;
            if (advance_on('|', s)) return TOKEN_LOGICAL_OR;
            return TOKEN_BIT_OR;
        case '}': return TOKEN_R_CURLY;
        default:
            if (is_ident_start(s->head[-1])) {
                return scan_word(s);
            } else if (s->head[-1] >= '0' && s->head[-1] <= '9') {
                return scan_numeric(s);
            }
            scan_error(
                s, "Unexpected character: '%s'", esc_chr(s->head[-1]).text
            );
    }
}

Token next_token(USE Scanner s) {
    if (setjmp(s->jump_buf)) {
        return (Token){
            .type = TOKEN_UNPARSEABLE,
            .line = s->line,
            .lexeme = get_lexeme(s),
        };
    }

    enum token_type type = next_token_type(s);

    String lexeme = get_lexeme(s);

    if (type == TOKEN_IDENT) {
        String ident = get_lexeme(s);
        intern(s->ident_pool, ident);
    } else {
        lexeme = get_lexeme(s);
    }

    return (Token){
        .type = type,
        .line = s->line,
        .lexeme = lexeme,
    };
}

void free_scanner([[clang::release_handle("scanner")]] Scanner s) { free(s); }
