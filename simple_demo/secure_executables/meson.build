test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../proto/defs.proto'
)
tm_simple_demo_boost_dep = dependency('boost', modules: ['program_options'])
tm_simple_demo_secure_data_source_exec = executable(
    'data_source'
    , ['DataSource.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_external_logic_lib, tm_simple_demo_security_logic_lib]
    , dependencies: [common_deps]
)
tm_simple_demo_secure_calculator_exec = executable(
    'calculator'
    , ['Calculator.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_external_logic_lib, tm_simple_demo_security_logic_lib]
    , dependencies: [common_deps]
)
tm_simple_demo_secure_main_logic_exec_cpp_flag = []
if build_machine.system() == 'windows' 
    tm_simple_demo_secure_main_logic_exec_cpp_flag += ['/bigobj']
endif
tm_simple_demo_secure_main_logic_exec = executable(
    'main_logic'
    , ['MainLogic.cpp', test_gen_out]
    , cpp_args: tm_simple_demo_secure_main_logic_exec_cpp_flag
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_program_logic_lib, tm_simple_demo_security_logic_lib]
    , dependencies: [common_deps, tm_simple_demo_boost_dep]
)

secure_gui_library = static_library(
    'secure_gui_library'
    , ['EnablerGUIDataFlow.cpp', test_gen_out]
    , include_directories: inc
    , dependencies: [common_deps]
)

executable(
    'fltk_enabler'
    , ['FltkEnablerGUI.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_security_logic_lib, secure_gui_library]
    , dependencies: [common_deps, dependency('fltk')]
    , gui_app: true
)

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg')
else
    wx = dependency('wxwidgets')
endif

executable(
    'wx_enabler'
    , ['WxEnablerGUI.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_security_logic_lib, secure_gui_library]
    , dependencies: [common_deps, wx]
    , gui_app: true
)

#FTXUI library is from https://github.com/ArthurSonzogni/FTXUI
#it can easily be built with cmake

executable(
    'ftxui_enabler'
    , ['FtxUIEnablerGUI.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_security_logic_lib, secure_gui_library]
    , dependencies: [common_deps, dependency('ftxui')]
)
executable(
    'token_auth_query_test'
    , ['TokenAuthQueryTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps, dependency('ftxui')]
)