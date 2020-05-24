test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../proto/defs.proto'
)
tm_simple_demo_boost_dep = dependency('boost', modules: ['program_options'])
tm_simple_demo_secure_calculator_exec = executable(
    'calculator'
    , ['Calculator.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_external_logic_lib, tm_simple_demo_security_logic_lib]
    , dependencies: [common_deps]
)
tm_simple_demo_secure_main_logic_exec_cpp_flag = []
if build_machine.system() == 'windows' 
    if host_machine.cpu_family() == 'x86'
        tm_simple_demo_secure_main_logic_exec_cpp_flag += ['/bigobj']
    endif
endif
tm_simple_demo_secure_main_logic_exec = executable(
    'main_logic'
    , ['MainLogic.cpp', test_gen_out]
    , cpp_args: tm_simple_demo_secure_main_logic_exec_cpp_flag
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_program_logic_lib, tm_simple_demo_security_logic_lib]
    , dependencies: [common_deps, tm_simple_demo_boost_dep]
)
