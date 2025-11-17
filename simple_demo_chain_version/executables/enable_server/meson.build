if build_machine.system() != 'windows'
  executable(
      'enable_server'
      , ['main.cpp']
      , include_directories: [inc]
      , dependencies: [common_deps, dependency('rocksdb'), dependency('boost', modules: ['program_options'])]
  )
endif
