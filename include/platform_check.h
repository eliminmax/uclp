/*
 * SPDX-FileCopyrightText: 2025 Eli Array Minkoff
 *
 * SPDX-License-Identifier: 0BSD
 */

// IWYU pragma: always_keep
#ifndef UCLP_C23_CHECKED
#define UCLP_C23_CHECKED
#ifndef __GNUC__
#error "Requires GNU C extensions"
#endif
#ifndef __x86_64__
#error "Only works on x86_64"
#endif
#if __STDC_VERSION__ < 202311L
#error "C23 required"
#endif
#endif
