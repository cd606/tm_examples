test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../proto/defs.proto'
)
tm_simple_demo_program_logic_lib = static_library(
    'tm_simple_demo_program_logic'
    , ['MainLogic.cpp', test_gen_out]
    , include_directories: [inc]
    , dependencies: [common_deps]
)