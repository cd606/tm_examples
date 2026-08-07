linenoise_inc = include_directories('linenoise-ng/include')
linenoise_sources = [
  'linenoise-ng/src/linenoise.cpp',
  'linenoise-ng/src/ConvertUTF.cpp',
  'linenoise-ng/src/wcwidth.cpp'
]
linenoise_lib = static_library('linenoise-ng', linenoise_sources,
  include_directories: linenoise_inc,
  cpp_args: ['-std=c++11']
)
linenoise_dep = declare_dependency(
  link_with: linenoise_lib,
  include_directories: linenoise_inc
)
