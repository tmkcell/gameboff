project(
  'gameboff',
  'c',
  version: '0.1.0',
  default_options : ['warning_level=3', 
    'buildtype=debugoptimized'])

cc = meson.get_compiler('c')

# args
pre_args = [
  '-DPKG_NAME="@0@"'.format(meson.project_name()),
  '-DPKG_VER="@0@"'.format(meson.project_version()),
]

if get_option('buildtype') == 'debugoptimized'
  pre_args += '-DDEBUG'
endif
if get_option('buildtype') != 'buildrelease'
  pre_args += '-DDEV=true'
endif

# either SDL3 or SDL2 can be used for this project
sdl = dependency('', required: false)
sdl2 = dependency('sdl2', required: false)
sdl3 = dependency('sdl3', required: false)
if sdl3.found()
  pre_args += '-DSDL3_DEP'
  sdl = sdl3
elif sdl2.found()
  pre_args += '-DSDL2_DEP'
  sdl = sdl2
else
  error('Ether SDL3 or SDL2 must be installed')
endif

add_project_arguments(pre_args, language: 'c')

subdir('src')
