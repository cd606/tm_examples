more_boost_dep = dependency('boost', modules: ['program_options'])
tm_listener_exe = executable(
    'tm_listener'
    , ['listener.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep]
)
tm_capturer_exe = executable(
    'tm_capturer'
    , ['capturer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep]
)
tm_historical_republisher_exe = executable(
    'tm_historical_republisher'
    , ['historical_republisher.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep]
)
tm_relayer_exe = executable(
    'tm_relayer'
    , ['relayer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep]
)