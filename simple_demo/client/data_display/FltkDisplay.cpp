#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Chart.H>

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
  constexpr size_t DATA_SIZE=500;
  std::array<double,DATA_SIZE> chartData;
  size_t count = 0;

  Fl_Double_Window *window = new Fl_Double_Window(840,510,"FLTK Input Data Display");
  Fl_Box *box = new Fl_Box(20,20,800,50,"Hello, World!");
  box->labelsize(16);
  Fl_Chart *chart = new Fl_Chart(20,90,800,400);
  chart->autosize(1);
  chart->bounds(0, 100.0);
  chart->type(FL_SPIKE_CHART);
  window->end();
  Fl::lock();
  window->show(argc, argv);

  TheEnvironment env;
  R r(&env);
  env.setLogFilePrefix("fltk_display", true);
  auto dataPrinter = M::pureExporter<simple_demo::InputData>(
    [box,chart,&count,&chartData](simple_demo::InputData &&d) {
        std::ostringstream oss;
        oss << "Value: " << std::fixed << std::setprecision(6) << d.value();
        Fl::lock();
        box->copy_label(oss.str().c_str());
        if (count < DATA_SIZE) {
          chartData[count] = d.value();
          ++count;
          chart->add(d.value(), nullptr, FL_RED);
        } else {
          std::memmove(&chartData[0], &chartData[1], (DATA_SIZE-1)*sizeof(double));
          chartData[DATA_SIZE-1] = d.value();
          chart->clear();
          for (auto const &x : chartData) {
            chart->add(x, nullptr, FL_RED);
          }
        }
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