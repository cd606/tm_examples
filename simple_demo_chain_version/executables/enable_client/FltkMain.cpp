#include "EnablerGUIDataFlow.hpp"

#ifdef _MSC_VER
#define WIN32
#endif

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

std::function<void(bool &&)> configureFeedFunc;
std::function<void()> exitFeedFunc;

void enableBtnCallback(Fl_Widget *) {
    configureFeedFunc(true);
}
void disableBtnCallback(Fl_Widget *) {
    configureFeedFunc(false);
}
void closeCallback(Fl_Widget *, void *) {
    exitFeedFunc();
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
    env.CustomizedExitControlComponent::operator=(
        CustomizedExitControlComponent(
            [window]() {
                Fl::lock();
                window->hide();
                Fl::unlock();
                Fl::awake();
            }
        )
    );
    R r(&env);

    auto configureImporterAndFunc = M::triggerImporter<bool>();
    auto configureImporter = std::get<0>(configureImporterAndFunc);
    r.registerImporter("configureImporter", configureImporter);
    configureFeedFunc = std::get<1>(configureImporterAndFunc);

    auto exitImporterAndFunc = M::constTriggerImporter<basic::VoidStruct>();
    auto exitImporter = std::get<0>(exitImporterAndFunc);
    r.registerImporter("exitImporter", exitImporter);
    exitFeedFunc = std::get<1>(exitImporterAndFunc);

    enableBtn->callback(enableBtnCallback);
    disableBtn->callback(disableBtnCallback);
    window->callback(closeCallback);

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
        , {r.importItem(exitImporter)}
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_enabler");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    int ret = Fl::run();
    return ret;
}