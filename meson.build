# SPDX-FileCopyrightText: 2025 Eli Array Minkoff
#
# SPDX-License-Identifier: GPL-3.0-only

project('uclp', 'c',
  version : '0.1',
  default_options : ['c_std=gnu23', 'warning_level=3'])

if meson.get_compiler('c').get_id() == 'clang'
  add_project_arguments('-Wno-nullability-extension', language: 'c')
else
  add_project_arguments('-D_Nonnull', '-D_Nullable', language: 'c')
endif

incdir = include_directories('include')

# need flex specifically to generate a reentrant parser
lex = find_program('flex')
lex_yy_c = custom_target('lex.yy.c',
  output : 'lex.yy.c',
  input : 'compiler/scan.lex',
  command : [lex, '-o@OUTPUT@', '@INPUT@', ],
)

executable('uclp_scanner',
  lex_yy_c,
  include_directories : incdir,
  c_args : ['-DSCANNER_STANDALONE'],
  install : false,
)

executable('ucf_loader',
  'resources/ucf_loader.c',
  include_directories: incdir,
  install : true,
)

loader_path = get_option('prefix') / get_option('bindir') / 'ucf_loader'
# This really should be done in a much simpler way, but meson custom_targets don't play
# nice with backslashes.
#
# If they did, command could be `['printf', ':ucf:M::\\xf8UCF::%s::\n', loader_path]`
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

executable('uclpc',
  lex_yy_c,
  'compiler/hash.c',
  'compiler/main.c',
  include_directories : incdir,
  install: true,
)
