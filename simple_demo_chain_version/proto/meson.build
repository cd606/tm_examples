tm_simple_demo_chain_version_generated = protobuf_gen.process(
    'defs.proto'
)
tm_simple_demo_chain_version_proto_lib = static_library(
    'tm_simple_demo_chain_version_proto'
    , [tm_simple_demo_chain_version_generated]
    , include_directories: [inc]
    , dependencies: [common_deps]
)