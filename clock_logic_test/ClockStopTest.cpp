#include <tm_kit/infra/WithTimeData.hpp>
#include <tm_kit/infra/RealTimeApp.hpp>
#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/infra/Environments.hpp>
#include <tm_kit/infra/DeclarativeGraph.hpp>

#include <tm_kit/basic/real_time_clock/ClockComponent.hpp>
#include <tm_kit/basic/AppClockHelper.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/basic/IntIDComponent.hpp>
#include <tm_kit/basic/VoidStruct.hpp>

using namespace dev::cd606::tm;
using Env = infra::Environment<
    infra::CheckTimeComponent<false>
    , infra::FlagExitControlComponent
    , basic::IntIDComponent<>
    , basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>
>;
using M = infra::RealTimeApp<Env>;
using R = infra::AppRunner<M>;

void f() {
    Env env;
    R r(&env);

    infra::DeclarativeGraph<R>("", {
        {"timer", basic::AppClockHelper<M>::Importer::createOneShotClockConstImporter<basic::VoidStruct>(env.now()+std::chrono::seconds(3), {})}
        , {"timerCB", [&env](basic::VoidStruct &&) {
            env.log(infra::LogLevel::Info, "Exiting");
            env.exit(0);
        }}
        , {"timer", "timerCB"}
    })(r);  

    env.log(infra::LogLevel::Info, "Starting");
    r.finalize();
    infra::terminationController(infra::RunForever {&env, std::chrono::milliseconds(500)});
    r.sendStopToAllNodes(std::chrono::seconds(1));
    env.log(infra::LogLevel::Info, "Successfully exited");
}

int main(int argc, char **argv) {
    for (int ii=0; ii<10; ++ii) {
        f();
    }
    return 0;
}