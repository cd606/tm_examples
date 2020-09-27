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
    'http_display_server'
    , ['HttpDisplayServer.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib]
    , dependencies: [common_deps]
)

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg')
else
    wx = dependency('wxwidgets')
endif

executable(
    'wx_display'
    , ['WxDisplay.cpp', test_gen_out]
    , include_directories: inc
    , link_with: [tm_simple_demo_proto_lib]
    , dependencies: [common_deps, wx]
    , gui_app: true
)
