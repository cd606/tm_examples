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

fltk_dep = dependency('fltk', required: false)
if fltk_dep.found()
    executable(
        'fltk_enabler'
        , ['FltkEnablerGUI.cpp', test_gen_out]
        , include_directories: inc
        , link_with: [tm_simple_demo_proto_lib, plain_gui_library]
        , dependencies: [common_deps, fltk_dep]
        , gui_app: true
    )
    message('Building fltk_enabler (FLTK found)')
else
    message('Skipping fltk_enabler (FLTK not found)')
endif

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg', required: false)
else
    wx = dependency('wxwidgets', required: false)
endif

if wx.found()
    executable(
        'wx_enabler'
        , ['WxEnablerGUI.cpp', test_gen_out]
        , include_directories: inc
        , link_with: [tm_simple_demo_proto_lib, plain_gui_library]
        , dependencies: [common_deps, wx]
        , gui_app: true
    )
    message('Building wx_enabler (wxWidgets found)')
else
    message('Skipping wx_enabler (wxWidgets not found)')
endif

#FTXUI library is from https://github.com/ArthurSonzogni/FTXUI
#it can easily be built with cmake

executable(
    'ftxui_enabler'
    , ['FtxUIEnablerGUI.cpp', test_gen_out, prebuild_ftxui_info_header]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib, plain_gui_library]
    , dependencies: [common_deps, dependency('ftxui')]
)
