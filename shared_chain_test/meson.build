shared_chain_test_dep = [
    dependency('libetcdcpp')
    , dependency('hiredis')
]
executable(
    'shared_chain_test'
    , ['SharedChainTest.cpp']
    , include_directories: inc
#    , link_args : ['-lrt']
    , dependencies: [common_deps, shared_chain_test_dep]
)