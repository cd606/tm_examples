#This implementation uses the following two libraries (MIT license):
# https://github.com/SergeyBel/AES.git
# https://github.com/msotoodeh/curve25519.git

tm_simple_demo_security_logic_lib = static_library(
    'tm_simple_demo_security_logic'
    , ['AESHook.cpp', 'SignatureHelper.cpp', 'DHHelper.cpp']
    , include_directories: [inc]
    , dependencies: [common_deps, dependency('curve25519'), dependency('AES')]
)
