project('TM kit examples', 'cpp', default_options: ['cpp_std=c++17'])

inc = include_directories('.')
tm_infra_dep = dependency('tm_kit_infra')
tm_basic_dep = dependency('tm_kit_basic')
tm_transport_dep = dependency('tm_kit_transport')
thread_dep = dependency('threads')
protoc = find_program('protoc')
protobuf_dep = dependency('protobuf')
redis_dep = [dependency('hiredis', required: get_option('redis'))]
common_deps = [tm_infra_dep, tm_basic_dep, tm_transport_dep, thread_dep, protobuf_dep, redis_dep]
if build_machine.system() == 'windows'
    if get_option('redis').enabled()
        common_deps += declare_dependency(link_args: 'shlwapi.lib')
    endif
endif
protobuf_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

subdir('clock_logic_test')
subdir('transport_utils')
subdir('simple_demo')
