#include "EnablerGUIDataFlow.hpp"

#ifdef _MSC_VER
#define WIN32
#endif

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

std::function<void(ConfigureCommand &&)> configureFeedFunc;

void enableBtnCallback(Fl_Widget *) {
    ConfigureCommand cmd;
    cmd.set_enabled(true);
    configureFeedFunc(std::move(cmd));
}
void disableBtnCallback(Fl_Widget *) {
    ConfigureCommand cmd;
    cmd.set_enabled(false);
    configureFeedFunc(std::move(cmd));
}

int main(int argc, char **argv) {
    Fl_Double_Window *window = new Fl_Double_Window(300,100,"FLTK Enabler");
    Fl_Box *status = new Fl_Box(100, 20, 100, 20, "");
    Fl_Button *enableBtn = new Fl_Button(20, 50, 120, 30, "Enable");
    Fl_Button *disableBtn = new Fl_Button(160, 50, 120, 30, "Disable");
    enableBtn->deactivate();
    disableBtn->deactivate();
    window->end();
    Fl::lock();
    window->show(argc, argv);

    TheEnvironment env;
    R r(&env);

    auto configureImporterAndFunc = M::triggerImporter<ConfigureCommand>();
    auto configureImporter = std::get<0>(configureImporterAndFunc);
    r.registerImporter("configureImporter", configureImporter);
    configureFeedFunc = std::get<1>(configureImporterAndFunc);

    enableBtn->callback(enableBtnCallback);
    disableBtn->callback(disableBtnCallback);

    auto statusHandler = M::pureExporter<bool>(
        [status,enableBtn,disableBtn](bool &&data) {
            static const char enabledLabel[] = "Enabled";
            static const char disabledLabel[] = "Disabled";
            Fl::lock();
            if (data) {
                status->label(enabledLabel);
                enableBtn->deactivate();
                disableBtn->activate();
            } else {
                status->label(disabledLabel);
                enableBtn->activate();
                disableBtn->deactivate();
            }
            Fl::unlock();
            Fl::awake();
        }
    );
    r.registerExporter("statusHandler", statusHandler);

    enablerGUIDataFlow(
        r
        , "fltk_enabler"
        , r.sourceAsSourceoid(r.importItem(configureImporter))
        , r.sinkAsSinkoid(r.exporterAsSink(statusHandler))
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_enabler");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    int ret = Fl::run();
    env.exit();
    return ret;
}