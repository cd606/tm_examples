more_boost_dep = dependency('boost', modules: ['program_options'])
extra_args = []
if get_option('redis').enabled()
    extra_args += '-DENABLE_REDIS_SUPPORT=1'
endif
tm_listener_exe = executable(
    'tm_listener'
    , ['listener.cpp']
    , cpp_args: extra_args
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_capturer_exe = executable(
    'tm_capturer'
    , ['capturer.cpp']
    , cpp_args: extra_args
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_historical_republisher_exe = executable(
    'tm_historical_republisher'
    , ['historical_republisher.cpp']
    , cpp_args: extra_args
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_relayer_exe = executable(
    'tm_relayer'
    , ['relayer.cpp']
    , cpp_args: extra_args
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)