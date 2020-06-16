#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/SinglePassIterationMonad.hpp>
#include <tm_kit/infra/IntIDComponent.hpp>

#include <tm_kit/basic/ByteData.hpp>
#include <tm_kit/basic/VoidStruct.hpp>
#include <tm_kit/basic/TrivialBoostLoggingComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockComponent.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockImporter.hpp>
#include <tm_kit/basic/single_pass_iteration_clock/ClockOnOrderFacility.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalTransactionHandlerComponent.hpp>
#include <tm_kit/basic/transaction/SingleKeyLocalStorageTransactionBroker.hpp>
#include <tm_kit/basic/transaction/InMemoryVersionProviderComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/BoostUUIDComponent.hpp>
#include <tm_kit/transport/SimpleIdentityCheckerComponent.hpp>

#include "TransactionHelpers.hpp"

#include <soci/soci.h>
#include <soci/sqlite3/soci-sqlite3.h>

#include <iostream>

#include <boost/program_options.hpp>

using namespace dev::cd606::tm;
using namespace db_one_list_subscription;

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

int main(int argc, char **argv) {
    namespace po = boost::program_options;

    po::options_description desc("allowed options");
    desc.add_options()
        ("help", "display help message")
        ("db_file", po::value<std::string>(), "database file")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("db_file")) {
        std::cerr << "Please provide database file\n";
        return 1;
    }
    
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

    TheEnvironment env;
    env.THComponent::operator=(THComponent {
        vm["db_file"].as<std::string>()
        , [&env](std::string const &s) {
            env.log(infra::LogLevel::Info, s);
        }
    });

    R r(&env); 

    auto facility = M::fromAbstractOnOrderFacility<TI::FacilityInput,TI::FacilityOutput>(
        new TB()
    );
    r.registerOnOrderFacility("facility", facility);

    auto extractor = M::kleisli<M::KeyedData<TI::FacilityInput,TI::FacilityOutput>>(
        basic::CommonFlowUtilComponents<M>::extractIDAndDataFromKeyedData<TI::FacilityInput,TI::FacilityOutput>()
    );
    auto convertToData = M::kleisli<M::Key<TI::FacilityOutput>>(
        basic::CommonFlowUtilComponents<M>::withKey<TI::FacilityOutput>(
            infra::KleisliUtils<M>::liftMaybe<TI::FacilityOutput>(
                basic::transaction::TITransactionFacilityOutputToData<
                    M, Key, Data, int64_t
                >()
            )
        )
    );
    auto unsubscriber = M::liftPure<M::Key<TI::OneValue>>(
        [](M::Key<TI::OneValue> &&data) -> TI::FacilityInput {
            return TI::FacilityInput {"", TI::BasicFacilityInput {
                { TI::Unsubscription {data.id(), Key{}} }
            }};
        }
    );
    auto dataExporter = M::pureExporter<M::Key<TI::OneValue>>(
        [&env](M::Key<TI::OneValue> &&data) {
            std::ostringstream oss;
            TI::OneValue tr = data.key();
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
    auto otherExporter = M::simpleExporter<M::Key<TI::FacilityOutput>>(
        [](M::InnerData<M::Key<TI::FacilityOutput>> &&data) {
            auto id = data.timedData.value.id();
            auto output = data.timedData.value.key();
            bool isFinal = data.timedData.finalFlag;
            auto *env = data.environment;

            switch (output.value.index()) {
            case 0:
                {
                    TI::TransactionResult const &tr = std::get<0>(output.value);
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
                }
                break;
            default:
                break;
            }
        }
    );

    auto importer = basic::single_pass_iteration_clock::template ClockImporter<TheEnvironment>
                    ::template createOneShotClockConstImporter<basic::VoidStruct>(
        infra::withtime_utils::parseLocalTime("2020-01-01T10:00:00")
        , basic::VoidStruct {}
    );

    auto createCommand = M::liftPure<basic::VoidStruct>(
        [](basic::VoidStruct &&) -> TI::FacilityInput {
            return TI::FacilityInput {"", TI::BasicFacilityInput {
                { TI::Subscription {Key{}} }
            }};
        }
    );

    auto keyify = M::kleisli<TI::FacilityInput>(
        basic::CommonFlowUtilComponents<M>::keyify<TI::FacilityInput>()
    );

    auto clockInput = r.importItem("importer", importer);

    r.placeOrderWithFacility(
        r.execute("keyify", keyify, 
            r.execute("createCommand", createCommand, 
                clockInput.clone()))
        , facility
        , r.actionAsSink("extrator", extractor)
    );
    r.exportItem("dataExporter", dataExporter, 
        r.execute("convertToData", convertToData, r.actionAsSource(extractor)));
    //Please note that as soon as keyify is hooked, we don't need to
    //call placeOrderWithFacility again, since the keyify'ed unsubscription
    //order will flow into the facility. If in doubt, look at the generated graph
    r.execute(keyify, r.execute("unsubscriber", unsubscriber, r.actionAsSource(convertToData)));
    r.exportItem("otherExporter", otherExporter, r.actionAsSource(extractor));

    basic::single_pass_iteration_clock::ClockOnOrderFacility<TheEnvironment>
        ::setupExitTimer(
        r 
        , std::chrono::hours(24)
        , clockInput.clone()
        , [](TheEnvironment *env) {
            env->log(infra::LogLevel::Info, "Wrapping up!");
        }
        , "exitTimerPart"
    );

    std::ostringstream graphOss;
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "db_one_list_single_pass_printer");

    env.log(infra::LogLevel::Info, graphOss.str());
    env.log(infra::LogLevel::Info, "DB one list single pass printer started");

    r.finalize();

    infra::terminationController(infra::ImmediatelyTerminate {});
}