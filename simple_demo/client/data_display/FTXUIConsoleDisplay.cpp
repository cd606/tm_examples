#include "DataDisplayFlow.hpp"

#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/event.hpp>

#include <sstream>
#include <iostream>

int main(int argc, char **argv) {
    using TheEnvironment = infra::Environment<
        infra::CheckTimeComponent<false>,
        infra::FlagExitControlComponent,
        transport::CrossGuidComponent,
        basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
        transport::AllNetworkTransportComponents
    >;
    using M = infra::RealTimeApp<TheEnvironment>;
    using R = infra::AppRunner<M>;

    TheEnvironment env;
    env.setLogFilePrefix("console_display", true);

    std::mutex mutex;
    std::vector<double> values;

    auto screen = ftxui::ScreenInteractive::FitComponent();

    auto renderer = ftxui::Renderer([&values,&mutex]() {
        std::lock_guard<std::mutex> _(mutex);
        auto c = ftxui::Canvas(500, 100);
        if (!values.empty()) {
            for (std::size_t ii=0; ii<values.size()-1 && ii<200; ++ii) {
                c.DrawPointLine(ii, 100-values[ii], ii+1, 100-values[ii+1], ftxui::Color::Red);
            }
            c.DrawText(0, 0, std::to_string(values.back()));
        }
        return ftxui::canvas(std::move(c));
    }) | ftxui::CatchEvent([&env,&screen](ftxui::Event e) -> bool {
        if (e == ftxui::Event::Escape) {
            screen.Post(screen.ExitLoopClosure());
            env.exit(0);
        }
        return true;
    });
    

    R r(&env);
    auto dataPrinter = M::pureExporter<simple_demo::InputData>(
        [&values,&mutex,&screen](simple_demo::InputData &&d) {
            std::lock_guard<std::mutex> _(mutex);
            if (values.size() < 200) {
                values.push_back(d.value());
            } else {
                for (std::size_t ii=0; ii<199; ++ii) {
                    values[ii] = values[ii+1];
                }
                values[199] = d.value();
            }
            screen.PostEvent(ftxui::Event::Custom);
        }
    );
    r.registerExporter("dataPrinter", dataPrinter);
    dataDisplayFlow<TheEnvironment>(
        r
        , r.sinkAsSinkoid(r.exporterAsSink(dataPrinter))
    );

    r.finalize();
    
    screen.Loop(renderer);

    env.log(infra::LogLevel::Info, "Exiting");
    r.sendStopToAllNodes(std::chrono::seconds(2));

    return 0;
}