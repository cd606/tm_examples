#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationMonad.hpp>
#include <tm_kit/infra/RealTimeMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/transaction/v2/TransactionLogicCombination.hpp>
#include <tm_kit/basic/transaction/v2/DataStreamClientCombination.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/MonadRunnerUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQOnOrderFacility.hpp>

#include "TransactionHelpers.hpp"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <iostream>
#include <atomic>

#include <boost/program_options.hpp>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

using DI = basic::transaction::v2::DataStreamInterface<
    int64_t
    , Key
    , int64_t
    , Data
    , int64_t
    , DataDelta
>;

template <class R> 
typename R::template Sink<basic::VoidStruct> dbSinglePassPrinterLogic(
      R &r
      , typename R::EnvironmentType &env
      , typename R::template FacilitioidConnector<
            typename basic::transaction::v2::GeneralSubscriberTypes<
                typename R::MonadType::EnvironmentType::IDType, DI
            >::Input
            , typename basic::transaction::v2::GeneralSubscriberTypes<
                typename R::MonadType::EnvironmentType::IDType, DI
            >::Output
        > queryConnector
  ) 
{
    using M = typename R::MonadType;
    
    using GS = basic::transaction::v2::GeneralSubscriberTypes<
        typename M::EnvironmentType::IDType, DI
    >;

    std::shared_ptr<int> unsubscriptionCount = std::make_shared<int>(0);
    r.preservePointer(unsubscriptionCount);

    auto printAck = M::template simpleExporter<typename M::template KeyedData<typename GS::Input, typename GS::Output>>(
        [&env,unsubscriptionCount](typename M::template InnerData<typename M::template KeyedData<typename GS::Input, typename GS::Output>> &&o) {
            auto id = o.timedData.value.key.id();
            std::visit([&id,&env,&unsubscriptionCount](auto const &x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T,typename GS::Subscription>) {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env.id_to_string(id)
                        << " on " << x.keys.size() << " keys";
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,typename GS::Unsubscription>) {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env.id_to_string(x.originalSubscriptionID)
                        << " from " << env.id_to_string(id);
                    env.log(infra::LogLevel::Info, oss.str());
                    ++(*unsubscriptionCount);
                    if (*unsubscriptionCount == 2) {
                        env.log(infra::LogLevel::Info, "All unsubscribed, exiting");
                        exit(0);
                    }
                } else if constexpr (std::is_same_v<T,typename GS::SubscriptionInfo>) {
                    std::ostringstream oss;
                    oss << "Got subscription info " << x;
                    env.log(infra::LogLevel::Info, oss.str());
                } else if constexpr (std::is_same_v<T,typename GS::UnsubscribeAll>) {
                    std::ostringstream oss;
                    oss << "Got unsubscribe-all ack from " << env.id_to_string(id);
                    env.log(infra::LogLevel::Info, oss.str());
                }
            }, o.timedData.value.data.value);
        }
    );

    auto printFullUpdate = M::template liftPure<typename M::template KeyedData<typename GS::Input, DI::FullUpdate>>(
        [&env](typename M::template KeyedData<typename GS::Input, DI::FullUpdate> &&update) -> typename M::EnvironmentType::IDType {
            std::ostringstream oss;
            oss << "Got full update {";
            oss << "globalVersion=" << update.data.version;
            oss << ",updates=[";
            int ii = 0;
            for (auto const &item : update.data.data) {
                if (ii > 0) {
                    oss << ",";
                }
                ++ii;
                oss << "{key=" << item.groupID;
                oss << ",version=" << item.version;
                if (item.data) {
                    oss << ",data=[";
                    int jj = 0;
                    for (auto const &row : *(item.data)) {
                        if (jj > 0) {
                            oss << ',';
                        }
                        ++jj;
                        oss << "{name='" << row.first.name << "'";
                        oss << ",amount=" << row.second.amount;
                        oss << ",stat=" << row.second.stat;
                        oss << "}";
                    }
                    oss << "]";
                } else {
                    oss << ",data=(deleted)";
                }
                oss << "}";
            }
            oss << "]";
            oss << "} (from " << env.id_to_string(update.key.id()) << ")";
            env.log(infra::LogLevel::Info, oss.str());

            return update.key.id();
        }
    );
    
    auto unsubscriber = M::template liftMaybe<typename M::EnvironmentType::IDType>(
        [](typename M::EnvironmentType::IDType &&id) -> std::optional<typename GS::Input> {
            static std::atomic<bool> unsubscribed = false;
            if (!unsubscribed) {
                unsubscribed = true;
                return typename GS::Input {
                    typename GS::Unsubscription {std::move(id)}
                };
            } else {
                return std::nullopt;
            }
        }
    );
   
    auto createCommand = M::template liftPure<basic::VoidStruct>(
        [](basic::VoidStruct &&) -> typename GS::Input {
            return typename GS::Input {
                typename GS::Subscription { std::vector<Key> {Key {}} }
            };
        }
    );
    auto keyify = M::template kleisli<typename GS::Input>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename GS::Input>()
    );

    auto keyedCommand = r.execute("keyify", keyify, r.actionAsSource("createCommand", createCommand));
    auto clientOutputs = basic::transaction::v2::dataStreamClientCombination<
        R, DI, typename GS::Input
        , basic::transaction::v2::TriviallyMerge<int64_t, int64_t>
        , ApplyDelta
    >(
        r 
        , "outputHandling"
        , queryConnector
        , std::move(keyedCommand)
    );
    r.exportItem("printAck", printAck, clientOutputs.rawSubscriptionOutputs.clone());
    //Please note that as soon as keyify is hooked, we don't need to
    //call queryConnector again, since the keyify'ed unsubscription
    //order will flow into the facility. If in doubt, look at the generated graph
    r.execute(keyify
        , r.execute("unsubscriber", unsubscriber
            , r.execute("printFullUpdate", printFullUpdate, clientOutputs.fullUpdates.clone())));

    return r.actionAsSink(createCommand);
}

template <class R> 
void printGraphAndRun(R &r, typename R::EnvironmentType &env) {
    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_one_list_single_pass_printer");

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB one list single pass printer starting");

    r.finalize();

    infra::terminationController(infra::RunForever {});
}

void runSinglePass(std::string const &dbFile) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<
            basic::single_pass_iteration_clock::ClockComponent<
                std::chrono::system_clock::time_point
            >
        >,
        infra::IntIDComponent<>
    >;

    using M = infra::SinglePassIterationMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    using GS = basic::transaction::v2::GeneralSubscriberTypes<
        typename M::EnvironmentType::IDType, DI
    >;

    TheEnvironment env;

    R r(&env);

    auto dbLoader = M::simpleImporter<DI::Update>(
        [&dbFile](TheEnvironment *env) -> M::Data<DI::Update> {
            auto session_ = std::make_shared<soci::session>(
#ifdef _MSC_VER
                *soci::factory_sqlite3()
#else
                soci::sqlite3
#endif
                , dbFile
            );
            Data initialData;
            soci::rowset<soci::row> res = 
                session_->prepare << "SELECT name, amount, stat FROM test_table";
            for (auto const &r : res) {
                db_key key;
                key.name = r.get<std::string>(0);
                db_data data;
                data.amount = r.get<int>(1);
                data.stat = r.get<double>(2);
                initialData.insert({key, data});
            }
            DI::Update update {
                0
                , std::vector<DI::OneUpdateItem> {
                    DI::OneFullUpdateItem {
                        Key {}
                        , 0
                        , std::move(initialData)
                    }
                }
            };
            return M::InnerData<DI::Update> {
                env
                , {
                    infra::withtime_utils::parseLocalTime("2020-01-01T09:00:00")
                    , std::move(update)
                    , true
                }
            };
        }
    );

    auto dataStoreForSubscriptionFacility =
        std::make_shared<basic::transaction::v2::TransactionDataStore<DI,std::hash<Key>,M::PossiblyMultiThreaded>>();

    using DM = basic::transaction::v2::TransactionDeltaMerger<
        DI, false, M::PossiblyMultiThreaded
        , basic::transaction::v2::TriviallyMerge<int64_t, int64_t>
        , ApplyDelta
    >;

    auto subscriptionFacility = basic::transaction::v2::subscriptionLogicCombination<
        R, DI, DM
    >(
        r
        , "subscription_server_components"
        , r.importItem("dbLoader", dbLoader)
        , dataStoreForSubscriptionFacility
    );

    auto initialImporter = basic::single_pass_iteration_clock::template ClockImporter<TheEnvironment>
                    ::template createOneShotClockConstImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , basic::VoidStruct {}
    );

    auto logicInterfaceSink = dbSinglePassPrinterLogic<R>(
        r
        , env
        , basic::MonadRunnerUtilComponents<R>::
            wrapTuple2FacilitioidBySupplyingDefaultValue<
                GS::Input, GS::Output, std::string
            >(
                R::localFacilityConnector(subscriptionFacility)
                , "wrap_tuple2_facility"
            )
    );

    r.connect(r.importItem("initialImporter", initialImporter), logicInterfaceSink);

    printGraphAndRun<R>(r, env);
}

void runRealTime() {   
    using GS = basic::transaction::v2::GeneralSubscriberTypes<
        typename boost::uuids::uuid, DI
    >;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , GS::Input>
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "db_one_list_subscription_client"
        )
    ); 

    R r(&env); 

    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <GS::Input,GS::Output>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue")
    );
    r.registerOnOrderFacility("facility", facility);

    auto initialImporter = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );
    auto logicInterfaceSink = dbSinglePassPrinterLogic<R>(
        r
        , env
        , R::facilityConnector(facility)
    );

    r.connect(r.importItem("initialImporter", initialImporter), logicInterfaceSink);

    printGraphAndRun<R>(r, env);
}

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("client", "run as client")
        ("db_file", po::value<std::string>(), "database file")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (vm.count("client")) {
        runRealTime();
    } else {
        if (!vm.count("db_file")) {
            std::cerr << "Please provide database file\n";
            return 1;
        }
    
        runSinglePass(vm["db_file"].as<std::string>());
    }
}