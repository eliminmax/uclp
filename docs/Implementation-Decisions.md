<!--
SPDX-FileCopyrightText: 2025 Eli Array Minkoff

SPDX-License-Identifier: GPL-3.0-only
-->

# Implementation Decisions

The primary motivation behind this project is to expand my knowledge and skill,
and that's shaping many of the design decisions I'm making. For instance, I'm
using `flex` but not `bison` or `yacc`, as I'm confident that I could make a
scanner myself, so taking a shortcut to having an (almost certainly better) one
is acceptable, but going in, I'm not sure how to create a parser, so I am not
using a generator tool for that.
