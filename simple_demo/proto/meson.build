tm_simple_demo_generated = protobuf_gen.process(
    'defs.proto'
)
tm_simple_demo_proto_lib = static_library(
    'tm_simple_demo_proto'
    , [tm_simple_demo_generated]
    , include_directories: [inc]
    , dependencies: [common_deps]
)