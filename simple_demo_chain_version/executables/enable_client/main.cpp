#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/component/button.hpp>
#include <ftxui/component/container.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include "EnablerGUIDataFlow.hpp"

#include <tm_kit/infra/TerminationController.hpp>
#include <tm_kit/transport/ExitDataSource.hpp>

class EnablerGUIComponent : public ftxui::Component {
private:
    std::function<void(bool &&)> configureFeedFunc_;
    std::function<void()> exitFunc_;
    std::wstring displayText_;
    ftxui::Button enableBtn_, disableBtn_, exitBtn_;
    ftxui::Container container_;
public:
    EnablerGUIComponent(std::function<void(bool &&)> const &configureFeedFunc, std::function<void()> const &exitFunc) 
        : ftxui::Component(), configureFeedFunc_(configureFeedFunc), exitFunc_(exitFunc)
        , displayText_(L"Unknown")
        , enableBtn_(L"Enable")
        , disableBtn_(L"Disable")
        , exitBtn_(L"Exit")
        , container_(ftxui::Container::Horizontal())
    {
        enableBtn_.on_click = [this]() {
            configureFeedFunc_(true);
        };
        disableBtn_.on_click = [this]() {
            configureFeedFunc_(false);
        };
        exitBtn_.on_click = [this]() {
            exitFunc_();
        };
        container_.Add(&enableBtn_);
        container_.Add(&disableBtn_);
        container_.Add(&exitBtn_);
        Add(&container_);
    }
    void setEnabled(bool b) {
        if (b) {
            displayText_ = L"Enabled";
        } else {
            displayText_ = L"Disabled";
        }
    }
    ftxui::Element Render() override {
        return ftxui::vbox({
            ftxui::text(displayText_) | ftxui::hcenter
            , ftxui::hbox({
                enableBtn_.Render()
                , disableBtn_.Render()
                , exitBtn_.Render()
            }) | ftxui::hcenter
        });
    }
};

int main(int argc, char **argv) {
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    TheEnvironment env;
    env.CustomizedExitControlComponent::operator=(
        CustomizedExitControlComponent(
            screen.ExitLoopClosure()
        )
    );
    R r(&env);

    auto configureImporterAndFunc = M::triggerImporter<bool>();
    auto configureImporter = std::get<0>(configureImporterAndFunc);
    r.registerImporter("configureImporter", configureImporter);
    auto configureFeedFunc = std::get<1>(configureImporterAndFunc);

    auto exitImporterAndFunc = M::constTriggerImporter<basic::VoidStruct>();
    auto exitImporter = std::get<0>(exitImporterAndFunc);
    r.registerImporter("exitImporter", exitImporter);
    auto exitFeedFunc = std::get<1>(exitImporterAndFunc);

    //For some reason my build of FTXUI will core dump when calling
    //destructor for this component, therefore I deliberately avoided
    //the destructor call
    auto *component = new EnablerGUIComponent(configureFeedFunc, exitFeedFunc);

    auto statusHandler = M::pureExporter<bool>(
        [component,&screen](bool &&data) {
            component->setEnabled(data);
            screen.PostEvent(ftxui::Event::Custom);
        }
    );

    r.registerExporter("statusHandler", statusHandler);

    enablerGUIDataFlow(
        r
        , "ftxui_enabler"
        , r.sourceAsSourceoid(r.importItem(configureImporter))
        , r.sinkAsSinkoid(r.exporterAsSink(statusHandler))
        , {r.importItem(exitImporter)}
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_enabler");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    component->TakeFocus();
    screen.Loop(component);

    return 0;
}
