test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../../proto/defs.proto'
)
my_link_args = ['-lrt']
if build_machine.system() == 'windows'
  my_link_args += ['-lws2_32']
endif
executable(
    'print_chain'
    , ['main.cpp', test_gen_out]
    , include_directories: [inc]
    , link_with: [tm_simple_demo_chain_version_proto_lib, tm_simple_demo_chain_version_main_program_logic_lib]
    , link_args : my_link_args
    , dependencies: [common_deps, dependency('libetcdcpp'), dependency('hiredis'), dependency('boost', modules: ['program_options']), dependency('libsodium')]
)