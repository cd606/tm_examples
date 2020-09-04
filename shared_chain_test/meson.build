shared_chain_test_dep = [
    dependency('libetcdcpp')
    , dependency('boost', modules: ['program_options'])
]
executable(
    'shared_chain_test'
    , ['SharedChainTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps, shared_chain_test_dep]
)