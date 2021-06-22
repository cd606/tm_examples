bcl_compat_test_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@/../proto/', '--cpp_out=@BUILD_DIR@', '@INPUT@'])
bcl_compat_test_gen_out = bcl_compat_test_gen.process(
    '../proto/bcl_compat_test.proto'
)
bcl_compat_test_cpp_server = executable(
    'cpp_server'
    , ['main.cpp', bcl_compat_test_gen_out]
    , include_directories: inc
    , link_with: [bcl_compat_test_proto_lib]
    , dependencies: [common_deps]
)