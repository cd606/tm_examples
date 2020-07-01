transaction_redundancy_test_dep = [
    dependency('linenoise-ng')
    , dependency('libetcdcpp')
    , dependency('boost', modules: ['program_options'])
    , dependency('google_cloud_cpp_common')
    , dependency('google_cloud_cpp_grpc_utils')
    , dependency('googleapis_cpp_api_http_protos')
    , dependency('googleapis_cpp_api_annotations_protos')
]
executable(
    'user_input_handler'
    , ['UserInputHandler.cpp', 'TransactionInterface.cpp']
    , include_directories: inc
    , dependencies: [common_deps, transaction_redundancy_test_dep]
)
# to make it easier to go between Linux and Windows
# , as of now, this example uses pre-generated protobuf 
# headers and sources
executable(
    'transaction_server'
    , ['TransactionServer.cpp', 'TransactionInterface.cpp', 'v3lock.grpc.pb.cc', 'v3lock.pb.cc']
    , include_directories: inc
    , dependencies: [common_deps, transaction_redundancy_test_dep]
)