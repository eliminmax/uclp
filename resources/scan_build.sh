#!/bin/sh

# SPDX-FileCopyrightText: 2026 Eli Array Minkoff
#
# SPDX-License-Identifier: 0BSD

# Meson doesn't preserve CC when invoking scan-build and causes problems.
#
# Put the path to this script in the SCANBUILD environment variable to work
# around that.
#
# see https://github.com/mesonbuild/meson/issues/5716

export CCC_CC=clang
exec scan-build "$@"
