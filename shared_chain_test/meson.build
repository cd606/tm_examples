shared_chain_test_dep = [
    dependency('libetcdcpp')
    , dependency('hiredis')
]
executable(
    'shared_chain_test'
    , ['SharedChainTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps, shared_chain_test_dep]
)