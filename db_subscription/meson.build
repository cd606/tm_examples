tm_db_subscription_generated = protobuf_gen.process(
    'dbData.proto'
)
soci_dep = dependency('soci')
tm_db_subscription_boost_dep = dependency('boost', modules: ['program_options'])
executable(
    'db_subscription_server'
    , ['DBSubscriptionServer.cpp', tm_db_subscription_generated]
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_subscription_boost_dep]
)
executable(
    'db_subscription_client'
    , ['DBSubscriptionClient.cpp', tm_db_subscription_generated]
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_subscription_boost_dep]
)
executable(
    'read_only_db_server'
    , ['ReadOnlyDBServer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_subscription_boost_dep]
)
executable(
    'read_only_db_client'
    , ['ReadOnlyDBClient.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'versionless_subscription_server'
    , ['VersionlessSubscriptionServer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_subscription_boost_dep]
)
executable(
    'versionless_subscription_client'
    , ['VersionlessSubscriptionClient.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_subscription_boost_dep]
)