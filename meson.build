project('TM kit examples', 'cpp', default_options: ['cpp_std=c++17'])

inc = include_directories('.')
tm_infra_dep = dependency('tm_kit_infra')
tm_basic_dep = dependency('tm_kit_basic')
tm_transport_dep = dependency('tm_kit_transport')
thread_dep = dependency('threads')
protoc = find_program('protoc')
protobuf_dep = dependency('protobuf')
redis_dep = [dependency('hiredis')]
nng_dep = [dependency('nngpp')]
boost_log_dep = dependency('boost', modules : ['log','log_setup', 'regex', 'random', 'system', 'thread', 'filesystem'])
spd_dep = dependency('spdlog')
crossguid_dep = dependency('crossguid')
rt_link_args = ['-lrt']
if build_machine.system() == 'windows'
  rt_link_args += ['-lws2_32']
endif
rt_dep = declare_dependency(link_args: rt_link_args)
common_deps = [tm_infra_dep, tm_basic_dep, tm_transport_dep, thread_dep, protobuf_dep, redis_dep, nng_dep, boost_log_dep, spd_dep, crossguid_dep, rt_dep]


protobuf_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

#add_global_arguments('-DBOOST_LOG_DYN_LINK', language: 'cpp')

if build_machine.system() == 'windows'
    add_global_arguments('/bigobj', language: 'cpp')
    add_global_arguments('/Zc:externC-', language: 'cpp')
endif

subdir('clock_logic_test')
subdir('common_flow_util_tests')
subdir('transport_utils')
subdir('simple_demo')
subdir('db_subscription')
subdir('db_one_list_subscription')
subdir('serialization_test')
subdir('transaction_redundancy_test')
subdir('shared_chain_test')
subdir('simple_demo_chain_version')
subdir('bcl_compat_test')
