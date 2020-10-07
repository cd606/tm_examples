#include "DataDisplayFlow.hpp"

#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <sstream>
#include <iostream>

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::TrivialExitControlComponent,
        transport::CrossGuidComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::AllNetworkTransportComponents
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.setLogFilePrefix("console_display", true);

    R r(&env);
    auto dataPrinter = M::pureExporter<simple_demo::InputData>(
        [](simple_demo::InputData &&d) {
            static bool firstTime = true;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << std::setw(10) << std::setfill(' ') << d.value();
            if (firstTime) {
                firstTime = false;
            } else {
                std::cout << "\x1b[G";
            }
            std::cout << "Value: " << oss.str() << std::flush;
        }
    );
    r.registerExporter("dataPrinter", dataPrinter);
    dataDisplayFlow<TheEnvironment>(
        r
        , r.sinkAsSinkoid(r.exporterAsSink(dataPrinter))
    );

    r.finalize();

    infra::terminationController(infra::TerminateAfterDuration {
        std::chrono::hours(1)
    });
}