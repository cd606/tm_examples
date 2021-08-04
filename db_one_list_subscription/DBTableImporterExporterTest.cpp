#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/BasicWithTimeApp.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>
#include <tm_kit/infra/SynchronousRunner.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>

#include <tm_kit/transport/db_table_importer_exporter/DBTableImporterFactory.hpp>
#include <tm_kit/transport/db_table_importer_exporter/DBTableExporterFactory.hpp>

#include <soci/sqlite3/soci-sqlite3.h>

#include "DBData.hpp"

using namespace dev::cd606::tm;

using Env = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;
using SR = infra::SynchronousRunner<M>;

#define TestDataFields \
    ((std::string, name)) \
    ((int32_t, amount)) \
    ((double, stat))
TM_BASIC_CBOR_CAPABLE_STRUCT(test_data, TestDataFields);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(test_data, TestDataFields);

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cerr << "Usage: db_table_importer_exporter_test DB_FILE [importer|importer-repeated|importer-repeated-with-key|exporter|exporter-batch|importer-sync|exporter-sync|exporter-batch-sync]\n";
        return -1;
    }
    auto session = std::make_shared<soci::session>(
#ifdef _MSC_VER
        *soci::factory_sqlite3()
#else
        soci::sqlite3
#endif
        , argv[1]
    );
    if (argc == 2 || std::string_view(argv[2]) == "importer") {
        Env env; 
        R r(&env);
        infra::DeclarativeGraph<R>("", {
            {"importer", transport::db_table_importer_exporter::DBTableImporterFactory<M>::createImporter<test_data>(session, "test_table")}
            , {"dispatch", basic::CommonFlowUtilComponents<M>::dispatchOneByOne<test_data>()}
            , {"print", [&env](test_data &&x) {
                std::ostringstream oss;
                oss << x;
                env.log(infra::LogLevel::Info, oss.str());
            }}
            , infra::DeclarativeGraphChain({"importer", "dispatch", "print"})
        })(r);
        r.finalize();
        infra::terminationController(infra::TerminateAfterDuration(std::chrono::seconds(1)));
    } else if (std::string_view(argv[2]) == "importer-repeated") {
        Env env; 
        R r(&env);
        infra::DeclarativeGraph<R>("", {
            {"importer", transport::db_table_importer_exporter::DBTableImporterFactory<M>::createRepeatedImporter<test_data>(session, "test_table", std::chrono::seconds(1))}
            , {"print", [&env](std::vector<test_data> &&x) {
                env.log(infra::LogLevel::Info, "=============================");
                std::ostringstream oss;
                for (auto const &r : x) {
                    oss.str("");
                    oss << r;
                    env.log(infra::LogLevel::Info, oss.str());
                }
            }}
            , {"importer", "print"}
        })(r);
        r.finalize();
        infra::terminationController(infra::RunForever {});
    } else if (std::string_view(argv[2]) == "importer-repeated-with-key") {
        Env env; 
        R r(&env);
        infra::DeclarativeGraph<R>("", {
            {"importer", transport::db_table_importer_exporter::DBTableImporterFactory<M>::createRepeatedImporterWithKeyCheck<test_data>(
                session
                , "test_table"
                , std::chrono::seconds(1)
                , [](test_data const &x) {
                    return x.name;
                }
            )}
            , {"print", [&env](std::vector<test_data> &&x) {
                env.log(infra::LogLevel::Info, "=============================");
                std::ostringstream oss;
                for (auto const &r : x) {
                    oss.str("");
                    oss << r;
                    env.log(infra::LogLevel::Info, oss.str());
                }
            }}
            , {"importer", "print"}
        })(r);
        r.finalize();
        infra::terminationController(infra::RunForever {});
    } else if (std::string_view(argv[2]) == "exporter") {
        Env env; 
        R r(&env);
        infra::DeclarativeGraph<R>("", {
            {"importer", M::constFirstPushImporter<std::vector<test_data>>({
                {"test1", 1, 1.2}, {"test2", 2, 2.3}
            })}
            , {"dispatch", basic::CommonFlowUtilComponents<M>::dispatchOneByOne<test_data>()}
            , {"insert", transport::db_table_importer_exporter::DBTableExporterFactory<M>::createExporter<test_data>(session, "test_table")}
            , infra::DeclarativeGraphChain({"importer", "dispatch", "insert"})
        })(r);
        r.finalize();
        infra::terminationController(infra::TerminateAfterDuration(std::chrono::seconds(1)));
    } else if (std::string_view(argv[2]) == "exporter-batch") {
        Env env; 
        R r(&env);
        infra::DeclarativeGraph<R>("", {
            {"importer", M::constFirstPushImporter<std::vector<test_data>>({
                {"test1", 1, 1.2}, {"test2", 2, 2.3}
            })}
            , {"insert", transport::db_table_importer_exporter::DBTableExporterFactory<M>::createBatchExporter<test_data>(session, "test_table")}
            , {"importer", "insert"}
        })(r);
        r.finalize();
        infra::terminationController(infra::TerminateAfterDuration(std::chrono::seconds(1)));
    } else if (std::string_view(argv[2]) == "importer-sync") {
        Env env; 
        SR r(&env);
        auto v = std::move(r.importItem(
            transport::db_table_importer_exporter::DBTableImporterFactory<M>::createImporter<test_data>(session, "test_table")
        )->front().timedData.value);
        for (auto const &x: v) {
            std::ostringstream oss;
            oss << x;
            env.log(infra::LogLevel::Info, oss.str());
        }
    } else if (std::string_view(argv[2]) == "exporter-sync") {
        Env env; 
        SR r(&env);
        auto ex = transport::db_table_importer_exporter::DBTableExporterFactory<M>::createExporter<test_data>(session, "test_table");
        std::vector<test_data> v {{"test1", 1, 1.2}, {"test2", 2, 2.3}};
        for (auto &&x: v) {
            r.exportItem(ex, std::move(x));
        }
    } else if (std::string_view(argv[2]) == "exporter-batch-sync") {
        Env env; 
        SR r(&env);
        auto ex = transport::db_table_importer_exporter::DBTableExporterFactory<M>::createBatchExporter<test_data>(session, "test_table");
        r.exportItem(ex, std::vector<test_data> {{"test1", 1, 1.2}, {"test2", 2, 2.3}});
    }
    return 0;
}