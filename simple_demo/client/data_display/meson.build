data_display_test_gen = generator(protoc, 
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@']
)
data_display_test_gen_out = data_display_test_gen.process(
    '../../proto/defs.proto'
)
executable(
    'console_display'
    , ['ConsoleDisplay.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib]
    , dependencies: [common_deps]
)
executable(
    'ftxui_console_display'
    , ['FTXUIConsoleDisplay.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib]
    , dependencies: [common_deps, dependency('ftxui')]
)
executable(
    'http_display_server'
    , ['HttpDisplayServer.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib]
    , dependencies: [common_deps]
)

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg', required: false)
else
    wx = dependency('wxwidgets', required: false)
endif

if wx.found()
    executable(
        'wx_display'
        , ['WxDisplay.cpp', test_gen_out]
        , include_directories: inc
        , link_with: [tm_simple_demo_proto_lib]
        , dependencies: [common_deps, wx]
        , gui_app: true
    )
    message('Building wx_display (wxWidgets found)')
else
    message('Skipping wx_display (wxWidgets not found)')
endif

fltk_dep = dependency('fltk', required: false)
if fltk_dep.found()
    executable(
        'fltk_display'
        , ['FltkDisplay.cpp', test_gen_out]
        , include_directories: inc
        , link_with: [tm_simple_demo_proto_lib]
        , dependencies: [common_deps, fltk_dep]
        , gui_app: true
    )
    message('Building fltk_display (FLTK found)')
else
    message('Skipping fltk_display (FLTK not found)')
endif
