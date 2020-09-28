#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#include "DataDisplayFlow.hpp"

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <sstream>
#include <iostream>

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    transport::CrossGuidComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;

int main(int argc, char **argv) {
  Fl_Window *window = new Fl_Window(340,180);
  Fl_Box *box = new Fl_Box(20,40,300,100,"Hello, World!");
  box->labelsize(36);
  window->end();
  Fl::lock();
  window->show(argc, argv);

  TheEnvironment env;
  R r(&env);
  env.setLogFilePrefix("fltk_display", true);
  auto dataPrinter = M::pureExporter<simple_demo::InputData>(
    [box](simple_demo::InputData &&d) {
        std::ostringstream oss;
        oss << "Value: " << std::fixed << std::setprecision(6) << d.value();
        Fl::lock();
        box->copy_label(oss.str().c_str());
        Fl::unlock();
        Fl::awake();
    }
  );
  r.registerExporter("dataPrinter", dataPrinter);
  dataDisplayFlow<TheEnvironment>(
    r
    , r.sinkAsSinkoid(r.exporterAsSink(dataPrinter))
  );
  r.finalize();
  
  int ret = Fl::run();
  env.exit();
  return ret;
}