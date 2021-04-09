soci_dep = dependency('soci')
tm_db_one_list_subscription_boost_dep = dependency('boost', modules: ['program_options'])
executable(
    'db_one_list_subscription_server'
    , ['DBOneListSubscriptionServer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
executable(
    'db_one_list_subscription_server_etcd'
    , ['DBOneListSubscriptionServerEtcd.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep, dependency('libetcdcpp')]
)
executable(
    'db_one_list_subscription_client'
    , ['DBOneListSubscriptionClient.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
executable(
    'db_one_list_single_pass_printer'
    , ['DBOneListSinglePassPrinter.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
executable(
    'read_only_db_one_list_server'
    , ['ReadOnlyDBOneListServer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
executable(
    'read_only_db_one_list_client'
    , ['ReadOnlyDBOneListClient.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'versionless_subscription_server'
    , ['VersionlessSubscriptionServer.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
executable(
    'versionless_subscription_client'
    , ['VersionlessSubscriptionClient.cpp']
    , include_directories: inc
    , dependencies: [common_deps, soci_dep, tm_db_one_list_subscription_boost_dep]
)
subdir('gui_client')