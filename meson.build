# SPDX-FileCopyrightText: 2025 - 2026 Eli Array Minkoff
#
# SPDX-License-Identifier: GPL-3.0-only

project('uclp', 'c',
  version : '0.1',
  default_options : ['c_std=gnu23', 'warning_level=3'])

if meson.get_compiler('c').get_id() == 'clang'
  add_project_arguments('-Wno-nullability-extension', language: 'c')
  add_project_arguments('-Wno-gnu-designator', language: 'c')
else
  add_project_arguments('-D_Nonnull', '-D_Nullable', language: 'c')
endif

incdir = include_directories('include')

mem = ['src/mem/arena.c', 'src/mem/checked.c', 'src/mem/dynarray.c']

hashtable = ['src/hashtable/hash.c', 'src/hashtable/table.c']

string_pool = ['src/util/string_pool.c']

parser = ['src/parser/scanner.c', 'src/parser/parser.c', 'src/parser/typeck.c']

cli = ['src/cli/colorize.c', 'src/cli/printerr.c']

uclpc = [ 'src/bins/uclpc.c', mem, hashtable, string_pool, parser, cli ]

ucf_loader = [ 'src/bins/ucf_loader.c' ]

executable('uclpc',
  uclpc,
  include_directories : incdir,
  install: true,
)

executable('uclpc.ubsan',
  uclpc,
  c_args: ['-fsanitize=undefined,address'],
  link_args: ['-fsanitize=undefined,address'],
  include_directories: incdir,
  install: false,
)

executable('ucf_loader',
  ucf_loader,
  include_directories: incdir,
  install : true,
)

loader_path = get_option('prefix') / get_option('bindir') / 'ucf_loader'

# This really should be done in a much simpler way, but meson custom_targets
# don't play nice with backslashes.
#
# If they did, command could be
# `['printf', ':ucf:M::\\xf8UCF::%s::\n', loader_path]`
#
# See https://github.com/mesonbuild/meson/issues/1564
custom_target('binfmt:ucf.conf',
  command : executable('gen_binfmt',
    configure_file(
      output : 'gen_binfmt.c',
      input : 'resources/gen_binfmt.c.in',
      configuration: {
        'INTERP' : loader_path
      },
      install : false,
    ),
    install : false
  ),
  output: 'ucf.conf',
  install_dir: '/etc/binfmt.d/',
  install: true,
  capture: true,
)
