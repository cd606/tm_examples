test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../proto/defs.proto'
)
tm_simple_demo_chain_version_calculator_logic_lib = static_library(
    'tm_simple_demo_chain_version_calculator_logic'
    , ['CalculatorStateFolder.cpp', 'CalculatorIdleWorker.cpp', test_gen_out]
    , include_directories: [inc]
    , dependencies: [common_deps]
)
subdir('typecheck')