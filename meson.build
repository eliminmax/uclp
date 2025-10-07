project('uclp', 'c',
  version : '0.1',
  default_options : ['c_std=gnu23', 'warning_level=3'])

incdir = include_directories('include')

executable('ucf_loader',
           'loader/ucf_loader.c',
           include_directories: incdir,
           install : true)
