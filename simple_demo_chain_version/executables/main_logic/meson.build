test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../../proto/defs.proto'
)
executable(
    'main_logic_integrated'
    , ['main_integrated.cpp', test_gen_out]
    , include_directories: [inc]
    , link_with: [tm_simple_demo_chain_version_proto_lib, tm_simple_demo_chain_version_external_logic_lib, tm_simple_demo_chain_version_main_program_logic_lib]
    , link_args : ['-lrt']
    , dependencies: [common_deps, dependency('libsodium')]
)
executable(
    'main_logic_request_generator'
    , ['main_request_generator.cpp', test_gen_out]
    , include_directories: [inc]
    , link_with: [tm_simple_demo_chain_version_proto_lib, tm_simple_demo_chain_version_external_logic_lib, tm_simple_demo_chain_version_main_program_logic_lib]
    , link_args : ['-lrt']
    , dependencies: [common_deps]
)
executable(
    'main_logic_request_placer'
    , ['main_request_placer.cpp', test_gen_out]
    , include_directories: [inc]
    , link_with: [tm_simple_demo_chain_version_proto_lib, tm_simple_demo_chain_version_external_logic_lib, tm_simple_demo_chain_version_main_program_logic_lib]
    , link_args : ['-lrt']
    , dependencies: [common_deps, dependency('libsodium')]
)