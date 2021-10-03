#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/basic/AppRunnerUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>
#include <tm_kit/transport/rabbitmq/RabbitMQComponent.hpp>
#include <tm_kit/transport/MultiTransportRemoteFacilityManagingUtils.hpp>
#include <tm_kit/transport/complex_key_value_store_components/SingleTableAsCollectionTransactionServer.hpp>

#include "ReadOnlyDBOneListData.hpp"

#include <soci/sqlite3/soci-sqlite3.h>

#include <iostream>
#include <atomic>

#include <boost/program_options.hpp>

using namespace dev::cd606::tm;

using TransactionServer =
    transport::complex_key_value_store_components::SingleTableAsCollectionTransactionServer<
        DBKey, DBData
    >;
using TI = TransactionServer::TI;
using DI = TransactionServer::DI;

template <class R> 
typename R::template Sink<basic::VoidStruct> dbSinglePassPrinterLogic(
      R &r
      , typename R::EnvironmentType &env
      , typename R::template FacilitioidConnector<
            typename basic::transaction::v2::GeneralSubscriberTypes<
                typename R::AppType::EnvironmentType::IDType, DI
            >::Input
            , typename basic::transaction::v2::GeneralSubscriberTypes<
                typename R::AppType::EnvironmentType::IDType, DI
            >::Output
        > queryConnector
  ) 
{
    using M = typename R::AppType;

    using GS = TransactionServer::GS<typename R::AppType::EnvironmentType::IDType>;

    auto printFullUpdate = M::template pureExporter<typename M::template KeyedData<typename GS::Input, DI::FullUpdate>>(
        [&env](typename M::template KeyedData<typename GS::Input, DI::FullUpdate> &&update) {
            std::ostringstream oss;
            oss << "Got full update {";
            oss << ",updates=[";
            int ii = 0;
            for (auto const &item : update.data.data) {
                if (ii > 0) {
                    oss << ",";
                }
                ++ii;
                oss << "{key=" << item.groupID;
                if (item.data) {
                    oss << ",data=[";
                    int jj = 0;
                    for (auto const &row : *(item.data)) {
                        if (jj > 0) {
                            oss << ',';
                        }
                        ++jj;
                        oss << "{" << row.first << "," << row.second << "}";
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
            env.exit();
        }
    );
      
    auto createCommand = M::template liftPure<basic::VoidStruct>(
        [](basic::VoidStruct &&) -> typename GS::Input {
            return typename GS::Input {
                typename GS::SnapshotRequest { std::vector<DI::Key> {DI::Key {}} }
            };
        }
    );
    auto keyify = M::template kleisli<typename GS::Input>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename GS::Input>()
    );

    auto keyedCommand = r.execute("keyify", keyify, r.actionAsSource("createCommand", createCommand));
    auto clientOutputs = basic::transaction::complex_key_value_store::as_collection::Combinations<R,DBKey,DBData>
        ::dataStreamClientCombinationFunc()(
        r 
        , "outputHandling"
        , queryConnector
        , std::move(keyedCommand)   
        , nullptr
    );
    r.exportItem("printFullUpdate", printFullUpdate, clientOutputs.fullUpdates.clone());

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

    infra::terminationController(infra::RunForever {&env});
}

void runSinglePass(std::string const &dbFile) {
    using GS = TransactionServer::GS<uint64_t>;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::FlagExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<
            basic::single_pass_iteration_clock::ClockComponent<
                std::chrono::system_clock::time_point
            >
        >,
        basic::IntIDComponent<>,
        TransactionServer::DSComponent,
        TransactionServer::THComponent
    >;

    using M = infra::SinglePassIterationApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;

    R r(&env);

    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , dbFile
    );
    TransactionServer::initializeEnvironment(&env, session, "test_table");

    auto transactionLogicCombinationRes = TransactionServer::setupTransactionServer(
        r
        , "transaction_server_components"
        , true //seal transaction facility
    );

    auto initialImporter = basic::single_pass_iteration_clock::template ClockImporter<TheEnvironment>
                    ::template createOneShotClockConstImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , basic::VoidStruct {}
    );

    auto logicInterfaceSink = dbSinglePassPrinterLogic<R>(
        r
        , env
        , basic::AppRunnerUtilComponents<R>::
            wrapTuple2FacilitioidBySupplyingDefaultValue<
                GS::Input, GS::Output, std::string
            >(
                R::localFacilityConnector(transactionLogicCombinationRes.subscriptionFacility)
                , "wrap_tuple2_facility"
            )
    );

    r.connect(r.importItem("initialImporter", initialImporter), logicInterfaceSink);

    printGraphAndRun<R>(r, env);
}

void runRealTime() { 
    using GS = TransactionServer::GS<boost::uuids::uuid>;  

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::rabbitmq::RabbitMQComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , GS::Input>
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,GS::Input>(
            "db_one_list_single_pass_printer"
        )
    ); 

    R r(&env); 

    auto facility = transport::MultiTransportRemoteFacilityManagingUtils<R>::setupSimpleRemoteFacilityWithProtocol
        <basic::CBOR,GS::Input,GS::Output>(
        r, "rabbitmq://127.0.0.1::guest:guest:test_db_one_list_cmd_subscription_queue_2"
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