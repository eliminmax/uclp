/*
 * SPDX-FileCopyrightText: 2026 Eli Array Minkoff
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UCLP_ANNOTATIONS
#define UCLP_ANNOTATIONS

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#define NODEREF
#if __has_attribute(__noderef__)
#undef NODEREF
#define NODEREF __attribute__((__noderef__))
#endif

#endif
