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

lex = find_program('flex', 'lex')
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
  'loader/ucf_loader.c',
  include_directories: incdir,
  install : true,
)
