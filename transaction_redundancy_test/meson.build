transaction_redundancy_test_dep = [
    dependency('linenoise-ng')
    , dependency('libetcdcpp')
]
executable(
    'user_input_handler'
    , ['UserInputHandler.cpp', 'TransactionInterface.cpp']
    , include_directories: inc
    , dependencies: [common_deps, transaction_redundancy_test_dep]
)
executable(
    'transaction_server'
    , ['TransactionServer.cpp', 'TransactionInterface.cpp']
    , include_directories: inc
    , dependencies: [common_deps, transaction_redundancy_test_dep]
)