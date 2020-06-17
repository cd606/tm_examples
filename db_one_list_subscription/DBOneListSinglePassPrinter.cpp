#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalTransactionHandlerComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalStorageTransactionBroker.hpp>
#include <tm_kit/basic/transaction/InMemoryVersionProviderComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

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

template <class R, class TI, class OutputToData> 
typename R::template Sink<basic::VoidStruct> dbSinglePassPrinterLogic(
      R &r
      , typename R::EnvironmentType &env
      , typename R::template FacilitioidConnector<
            typename TI::BasicFacilityInput
            , typename TI::FacilityOutput
        > queryConnector
  ) 
{
    using M = typename R::MonadType;
    
    //suggestThreaded is optional because OnOrderFacility uses recursive
    //mutexes. If OnOrderFacility uses simple mutexes, then we must have
    //suggestThreaded here, otherwise, the graph loop will be modifying the 
    //key list in the OnOrderFacility from within an OnOrderFacility callback,
    //and the simple mutex will deadlock upon reentrace.
    auto extractor = M::template kleisli<typename M::template KeyedData<typename TI::BasicFacilityInput,typename TI::FacilityOutput>>(
        basic::CommonFlowUtilComponents<M>::template extractIDAndDataFromKeyedData<typename TI::BasicFacilityInput,typename TI::FacilityOutput>()
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            //.SuggestThreaded(true)
            .DelaySimulator(
                [](int which, std::chrono::system_clock::time_point const &) -> std::chrono::system_clock::duration {
                    return std::chrono::milliseconds(100);
                }
            )
    );
    auto convertToData = M::template kleisli<typename M::template Key<typename TI::FacilityOutput>>(
        basic::CommonFlowUtilComponents<M>::template withKey<typename TI::FacilityOutput>(
            infra::KleisliUtils<M>::template liftMaybe<typename TI::FacilityOutput>(
                OutputToData()
            )
        )
    );
    auto unsubscriber = M::template liftMaybe<typename M::template Key<typename TI::OneValue>>(
        [](typename M::template Key<typename TI::OneValue> &&data) -> std::optional<typename TI::BasicFacilityInput> {
            static std::atomic<bool> unsubscribed = false;
            if (!unsubscribed) {
                unsubscribed = true;
                return typename TI::BasicFacilityInput {
                    { typename TI::Unsubscription {data.id(), Key{}} }
                };
            } else {
                return std::nullopt;
            }
        }
    );
    auto dataExporter = M::template pureExporter<typename M::template Key<typename TI::OneValue>>(
        [&env](typename M::template Key<typename TI::OneValue> &&data) {
            std::ostringstream oss;
            typename TI::OneValue tr = data.key();
            if (tr.data) {
                oss << "Current data: [";
                for (auto const &item : *(tr.data)) {
                    oss << "{name:'" << item.first.name << "'"
                            << ",amount:" << item.second.amount
                            << ",stat:" << item.second.stat
                            << "} ";
                }
                oss << "]";
                oss << " (size: " << tr.data->size() << ")";
                oss << " (version: " << tr.version << ")";
                oss << " (id: " << data.id() << ")";
            } else {
                oss << "Current data was deleted";
            }
            env.log(infra::LogLevel::Info, oss.str());
        }
    );
    auto otherExporter = M::template simpleExporter<typename M::template Key<typename TI::FacilityOutput>>(
        [](typename M::template InnerData<typename M::template Key<typename TI::FacilityOutput>> &&data) {
            static int unsubscriptionCount = 0;

            auto id = data.timedData.value.id();
            auto output = data.timedData.value.key();
            bool isFinal = data.timedData.finalFlag;
            auto *env = data.environment;

            switch (output.value.index()) {
            case 0:
                {
                    typename TI::TransactionResult const &tr = std::get<0>(output.value);
                    std::ostringstream oss;
                    switch (tr.index()) {
                    case 0:
                        oss << "Got transaction success for " << env->id_to_string(id);
                        break;
                    case 1:
                        oss << "Got transaction failure by permission for " << env->id_to_string(id);
                        break;
                    case 2:
                        oss << "Got transaction failure by precondition for " << env->id_to_string(id);
                        break;
                    case 3:
                        oss << "Got transaction handled asynchronously for " << env->id_to_string(id);
                        break;
                    default:
                        oss << "Got unknown transaction failure for " << env->id_to_string(id);
                        break;
                    }
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 1:
                {
                    std::ostringstream oss;
                    oss << "Got subscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                }
                break;
            case 2:
                {
                    std::ostringstream oss;
                    oss << "Got unsubscription ack for " << env->id_to_string(id);
                    if (isFinal) {
                        oss << " [F]";
                    }
                    env->log(infra::LogLevel::Info, oss.str());
                    ++unsubscriptionCount;
                    if (unsubscriptionCount == 2) {
                        env->log(infra::LogLevel::Info, "All unsubscribed, exiting");
                        exit(0);
                    }
                }
                break;
            default:
                break;
            }
        }
    );
    auto createCommand = M::template liftPure<basic::VoidStruct>(
        [](basic::VoidStruct &&) -> typename TI::BasicFacilityInput {
            return typename TI::BasicFacilityInput {
                { typename TI::Subscription {Key{}} }
            };
        }
    );
    auto keyify = M::template kleisli<typename TI::BasicFacilityInput>(
        basic::CommonFlowUtilComponents<M>::template keyify<typename TI::BasicFacilityInput>()
    );

    queryConnector(
        r
        , r.execute("keyify", keyify, 
            r.actionAsSource("createCommand", createCommand)) 
        , r.actionAsSink("extrator", extractor)
    );
    r.exportItem("dataExporter", dataExporter, 
        r.execute("convertToData", convertToData, r.actionAsSource(extractor)));
    //Please note that as soon as keyify is hooked, we don't need to
    //call queryConnector again, since the keyify'ed unsubscription
    //order will flow into the facility. If in doubt, look at the generated graph
    r.execute(keyify, r.execute("unsubscriber", unsubscriber, r.actionAsSource(convertToData)));
    r.exportItem("otherExporter", otherExporter, r.actionAsSource(extractor));

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
    class THComponent : public basic::transaction::ReadOnlySingleKeyLocalTransactionHandlerComponent<
        Key, Data
    > {
    private:
        std::string dbFile_;
        std::function<void(std::string)> logger_;
    public:
        THComponent() : dbFile_(), logger_() {
        }
        THComponent(std::string const &dbFile, std::function<void(std::string)> const &logger) : dbFile_(dbFile), logger_(logger) {
        }
        THComponent(THComponent &&c) = default;
        THComponent &operator=(THComponent &&c) = default;
        virtual ~THComponent() {
        }
        virtual std::vector<std::tuple<Key,Data>> loadInitialData() override final {
            soci::session session(
#ifdef _MSC_VER
                *soci::factory_sqlite3()
#else
                soci::sqlite3
#endif
                , dbFile_
            );
            std::vector<std::tuple<Key,Data>> ret;
            ret.resize(1);
            soci::rowset<soci::row> res = 
                session.prepare << "SELECT name, amount, stat FROM test_table";
            for (auto const &r : res) {
                db_key key;
                key.name = r.get<std::string>(0);
                db_data data;
                data.amount = r.get<int>(1);
                data.stat = r.get<double>(2);
                std::get<1>(ret[0]).insert({key, data});
            }
            std::ostringstream oss;
            oss << "[THComponent] loaded " << std::get<1>(ret[0]).size() << " rows";
            logger_(oss.str());
            return ret;
        }
    };
    
    using TI = basic::transaction::SingleKeyTransactionInterface<
        Key
        , Data
        , int64_t //version
        , uint64_t //ID
    >;

    using VP = basic::transaction::InMemoryVersionProviderComponent<
        Key
        , Data
    >;

    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<
            basic::single_pass_iteration_clock::ClockComponent<
                std::chrono::system_clock::time_point
            >
        >,
        infra::IntIDComponent<>,
        VP,
        THComponent
    >;
    using M = infra::SinglePassIterationMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;

    using TB = basic::transaction::SingleKeyLocalStorageTransactionBroker<
        M
        , Key
        , Data
        , int64_t
    >;
    using OutputToData = basic::transaction::TITransactionFacilityOutputToData<
        M, Key, Data, int64_t
    >;

    TheEnvironment env;
    env.THComponent::operator=(THComponent {
        dbFile
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });

    R r(&env); 

    auto innerFacility = M::fromAbstractOnOrderFacility<TI::FacilityInput,TI::FacilityOutput>(
        new TB()
    );
    //The reason we have to wrap for single pass is that
    //the transaction broker wants a TI::FacilityInput 
    //(which is a TI::BasicFacilityInput with a string account)
    //, in RealTimeMonad, we use identity attacher to
    //attach this account, but here we have to add by ourselves
    auto facility = M::wrappedOnOrderFacility<
        TI::BasicFacilityInput, TI::FacilityOutput
        , TI::FacilityInput, TI::FacilityOutput
    >(
        std::move(*innerFacility)
        , std::move(*(M::template kleisli<typename M::template Key<TI::BasicFacilityInput>>(
            basic::CommonFlowUtilComponents<M>::withKey<TI::BasicFacilityInput>(
                infra::KleisliUtils<M>::liftPure<TI::BasicFacilityInput>(
                    [](TI::BasicFacilityInput &&x) -> TI::FacilityInput {
                        return {"", std::move(x)};
                    }
                )
            )
        )))
        , std::move(*(M::template kleisli<typename M::template Key<TI::FacilityOutput>>(
            basic::CommonFlowUtilComponents<M>::idFunc<typename M::template Key<TI::FacilityOutput>>()
        )))
    );
    r.registerOnOrderFacility("facility", facility); 

    auto importer = basic::single_pass_iteration_clock::template ClockImporter<TheEnvironment>
                    ::template createOneShotClockConstImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , basic::VoidStruct {}
    );

    auto logicInterfaceSink = dbSinglePassPrinterLogic<R,TI,OutputToData>(
        r
        , env
        , R::facilityConnector(facility)
    );

    r.connect(r.importItem("importer", importer), logicInterfaceSink);

    printGraphAndRun<R>(r, env);
}

void runRealTime() {
    using TI = basic::transaction::SingleKeyTransactionInterface<
        Key
        , Data
        , int64_t
        , transport::BoostUUIDComponent::IDType
        , DataSummary
        , DataDelta
    >;
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        basic::TimeComponentEnhancedWithBoostTrivialLogging<basic::real_time_clock::ClockComponent>,
        transport::BoostUUIDComponent,
        transport::ClientSideSimpleIdentityAttacherComponent<
            std::string
            , TI::BasicFacilityInput>,
        transport::rabbitmq::RabbitMQComponent
    >;
    using M = infra::RealTimeMonad<TheEnvironment>;
    using R = infra::MonadRunner<M>;
    using OutputToData = basic::transaction::TITransactionFacilityOutputToData<
        M, Key, Data, int64_t
        , false //not mutex protected
        , DataSummary, CheckSummary, DataDelta, ApplyDelta
    >;

    TheEnvironment env;
    env.transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>::operator=(
        transport::ClientSideSimpleIdentityAttacherComponent<std::string,TI::BasicFacilityInput>(
            "db_one_list_single_pass_printer"
        )
    );

    R r(&env); 

    auto facility = transport::rabbitmq::RabbitMQOnOrderFacility<TheEnvironment>::WithIdentity<std::string>::createTypedRPCOnOrderFacility
        <TI::BasicFacilityInput,TI::FacilityOutput>(
        transport::ConnectionLocator::parse("127.0.0.1::guest:guest:test_db_one_list_cmd_queue")
    );
    r.registerOnOrderFacility("facility", facility);

    auto importer = M::simpleImporter<basic::VoidStruct>(
        [](M::PublisherCall<basic::VoidStruct> &p) {
            p(basic::VoidStruct {});
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );

    auto logicInterfaceSink = dbSinglePassPrinterLogic<R,TI,OutputToData>(
        r
        , env
        , R::facilityConnector(facility)
    );

    r.connect(r.importItem("importer", importer), logicInterfaceSink);

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