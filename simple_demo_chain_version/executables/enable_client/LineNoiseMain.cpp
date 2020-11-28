#include "EnablerGUIDataFlow.hpp"

#include <linenoise.h>

#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/CommonFlowUtils.hpp>
#include <tm_kit/transport/ExitDataSource.hpp>

int main(int argc, char **argv) {
    TheEnvironment env;
    env.CustomizedExitControlComponent::operator=(
        CustomizedExitControlComponent(
            []() {
                ::exit(0);
            }
        )
    );
    R r(&env);

    auto lineImporter = M::simpleImporter<std::string>(
        [](M::PublisherCall<std::string> &pub) {
            linenoiseHistorySetMaxLen(100);
            char *p;
            while ((p = linenoise(">>")) != nullptr) {
                if (p[0] != '\0') {
                    linenoiseHistoryAdd(p);
                }
                pub(boost::trim_copy(std::string {p}));
            }
            pub("exit");
        }
        , infra::LiftParameters<std::chrono::system_clock::time_point>()
            .SuggestThreaded(true)
    );
    auto share = infra::KleisliUtils<M>::action(basic::CommonFlowUtilComponents<M>::shareBetweenDownstream<std::string>());
    auto translateToConfig = M::liftMaybe<std::shared_ptr<std::string const>>(
        [](std::shared_ptr<std::string const> &&s) -> std::optional<bool> {
            if (*s == "enable" || *s == "on") {
                return true;
            } else if (*s == "disable" || *s == "off") {
                return false;
            } else {
                return std::nullopt;
            }
        }
    );
    auto translateToExit = M::liftMaybe<std::shared_ptr<std::string const>>(
        [](std::shared_ptr<std::string const> &&s) -> std::optional<basic::VoidStruct> {
            if (*s == "exit") {
                return basic::VoidStruct {};
            } else {
                return std::nullopt;
            }
        }
    );
    auto exitMerger = infra::KleisliUtils<M>::action(basic::CommonFlowUtilComponents<M>::idFunc<basic::VoidStruct>());
    auto showStatus = M::simpleExporter<bool>(
        [](M::InnerData<bool> &&status) {
            status.environment->log(infra::LogLevel::Info, (status.timedData.value?"Enabled":"Disabled"));
        }
    );

    r.registerImporter("lineImporter", lineImporter);
    r.registerAction("share", share);
    r.registerAction("translateToConfig", translateToConfig);
    r.registerAction("translateToExit", translateToExit);
    r.registerAction("exitMerger", exitMerger);
    r.registerExporter("showStatus", showStatus);

    auto strSrc = r.execute(share, r.importItem(lineImporter));
    auto exitSource = transport::ExitDataSourceCreator::addExitDataSource(
        r, "interrupt"
    );
    r.execute(exitMerger, std::move(exitSource));
    r.execute(exitMerger, r.execute(translateToExit, strSrc.clone()));

    enablerGUIDataFlow(
        r
        , "linenoise_enabler"
        , r.sourceAsSourceoid(r.execute(translateToConfig, strSrc.clone()))
        , r.sinkAsSinkoid(r.exporterAsSink(showStatus))
        , {r.actionAsSource(exitMerger)}
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_enabler");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    infra::terminationController(infra::TerminateAtTimePoint {
        infra::withtime_utils::parseLocalTodayActualTime(23, 59, 59)
    });
}