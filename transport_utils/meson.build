more_boost_dep = dependency('boost', modules: ['program_options'])
tm_listener_exe = executable(
    'tm_listener'
    , ['listener.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_capturer_exe = executable(
    'tm_capturer'
    , ['capturer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_historical_republisher_exe = executable(
    'tm_historical_republisher'
    , ['historical_republisher.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_relayer_exe = executable(
    'tm_relayer'
    , ['relayer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_delay_measurer_exe = executable(
    'tm_delay_measurer'
    , ['delay_measurer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
byte_data_chain_transcriber_link_args = ['-lrt']
if build_machine.system() == 'windows'
  byte_data_chain_transcriber_link_args += ['-lws2_32']
endif
tm_byte_data_chain_transcriber_exe = executable(
    'tm_byte_data_chain_transcriber'
    , ['byte_data_chain_transcriber.cpp']
    , include_directories: [inc]
    , link_args : byte_data_chain_transcriber_link_args
    , dependencies: [common_deps, dependency('libetcdcpp'), dependency('hiredis'), dependency('tclap')]
)
tm_byte_data_chain_publisher_exe = executable(
    'tm_byte_data_chain_publisher'
    , ['byte_data_chain_publisher.cpp']
    , include_directories: [inc]
    , link_args : byte_data_chain_transcriber_link_args
    , dependencies: [common_deps, dependency('libetcdcpp'), dependency('hiredis'), dependency('tclap')]
)
tm_byte_data_chain_append_from_broadcast_exe = executable(
    'tm_byte_data_chain_append_from_broadcast'
    , ['byte_data_chain_append_from_broadcast.cpp']
    , include_directories: [inc]
    , link_args : byte_data_chain_transcriber_link_args
    , dependencies: [common_deps, dependency('libetcdcpp'), dependency('hiredis'), dependency('tclap')]
)
tm_rpc_delay_measurer_exe = executable(
    'tm_rpc_delay_measurer'
    , ['rpc_delay_measurer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)
tm_sender_exe = executable(
    'tm_sender'
    , ['sender.cpp']
    , include_directories: inc
    , dependencies: [common_deps, more_boost_dep, dependency('libzmq')]
)