project('TM kit examples', 'cpp')
cpp = meson.get_compiler('cpp')
add_project_arguments(
  cpp.get_supported_arguments('/std:c++latest', '-std=c++2a'),
  language: 'cpp'
)

inc = include_directories('.')
if get_option('buildtype') == 'debug'
  tm_infra_dep = dependency('tm_kit_infra_debug')
  tm_basic_dep = dependency('tm_kit_basic_debug')
  tm_transport_dep = dependency('tm_kit_transport_debug')
else
  tm_infra_dep = dependency('tm_kit_infra')
  tm_basic_dep = dependency('tm_kit_basic')
  tm_transport_dep = dependency('tm_kit_transport')
endif
thread_dep = dependency('threads')
protoc = find_program('protoc')
protobuf_dep = dependency('protobuf')
redis_dep = [dependency('hiredis')]
nng_dep = [dependency('nngpp')]
boost_modules = ['log', 'log_setup', 'random', 'thread', 'filesystem']

# Try adding 'system'
boost_log_dep = dependency('boost',
  modules : boost_modules + ['system'],
  required : false
)

# If Boost with 'system' not found, retry without it
if not boost_log_dep.found()
  boost_log_dep = dependency('boost',
    modules : boost_modules,
    required : true
  )
endif
spd_dep = dependency('spdlog')
crossguid_dep = dependency('crossguid')
nlohmann_json_dep = dependency('nlohmann-json')
rt_link_args = ['-lrt']
if build_machine.system() == 'windows'
  rt_link_args += ['-lws2_32']
endif
rt_dep = declare_dependency(link_args: rt_link_args)
common_deps = [tm_infra_dep, tm_basic_dep, tm_transport_dep, thread_dep, protobuf_dep, redis_dep, nng_dep, boost_log_dep, spd_dep, crossguid_dep, nlohmann_json_dep, rt_dep]


protobuf_gen = generator(protoc, \
  output    : ['@BASENAME@.pb.cc', '@BASENAME@.pb.h'],
  arguments : ['--proto_path=@CURRENT_SOURCE_DIR@', '--cpp_out=@BUILD_DIR@', '@INPUT@'])

#add_global_arguments('-DBOOST_LOG_DYN_LINK', language: 'cpp')

if build_machine.system() == 'windows'
    common_deps += declare_dependency(link_args: 'bcrypt.lib')
    common_deps += declare_dependency(link_args: 'crypt32.lib')
    add_global_arguments('/bigobj', language: 'cpp')
    add_global_arguments('/Zc:externC-', language: 'cpp')
    #add_global_arguments('-DWINVER=0x0601', language: 'cpp')
    #add_global_arguments('-D_WIN32_WINNT=0x0601', language: 'cpp')
    #add_global_arguments('-DBOOST_USE_WINAPI_VERSION=BOOST_WINAPI_VERSION_WIN7', language: 'cpp')
    add_global_link_arguments('/NODEFAULTLIB:libcmt.lib', language: 'cpp')
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
subdir('rpc_examples')
subdir('csv_test')
subdir('grpc_interop_test')
subdir('json_rest_test')
subdir('websocket_test')
subdir('synthetic_facility_test')
subdir('hdf5_test')
