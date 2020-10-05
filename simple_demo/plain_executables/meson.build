test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
test_gen_out = test_gen.process(
    '../proto/defs.proto'
)
tm_simple_demo_boost_dep = dependency('boost', modules: ['program_options'])
tm_simple_demo_data_source_exec = executable(
    'data_source'
    , ['DataSource.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_external_logic_lib]
    , dependencies: [common_deps]
)
tm_simple_demo_calculator_exec = executable(
    'calculator'
    , ['Calculator.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_external_logic_lib]
    , dependencies: [common_deps]
)
tm_simple_demo_main_logic_exec_cpp_flag = []
if build_machine.system() == 'windows' 
    tm_simple_demo_main_logic_exec_cpp_flag += ['/bigobj']
endif
tm_simple_demo_main_logic_exec = executable(
    'main_logic'
    , ['MainLogic.cpp', test_gen_out]
    , cpp_args: tm_simple_demo_main_logic_exec_cpp_flag
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, tm_simple_demo_program_logic_lib]
    , dependencies: [common_deps, tm_simple_demo_boost_dep]
)

plain_gui_library = static_library(
    'plain_gui_library'
    , ['EnablerGUIDataFlow.cpp', test_gen_out]
    , include_directories: inc
    , dependencies: [common_deps]
)

executable(
    'fltk_enabler'
    , ['FltkEnablerGUI.cpp']
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, plain_gui_library]
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
    , ['WxEnablerGUI.cpp']
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, plain_gui_library]
    , dependencies: [common_deps, wx]
    , gui_app: true
)
