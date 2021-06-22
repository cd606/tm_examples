bcl_compat_test_generated = protobuf_gen.process(
    'bcl_compat_test.proto'
)
bcl_compat_test_proto_lib = static_library(
    'bcl_compat_test_proto'
    , [bcl_compat_test_generated]
    , include_directories: [inc]
    , dependencies: [common_deps]
)