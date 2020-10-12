#include "EnablerGUIDataFlow.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <ftxui/component/button.hpp>
#include <ftxui/component/container.hpp>
#include <ftxui/component/screen_interactive.hpp>

class EnablerGUIComponent : public ftxui::Component {
private:
    std::function<void(ConfigureCommand &&)> configureFeedFunc_;
    std::wstring displayText_;
    ftxui::Button enableBtn_, disableBtn_;
    ftxui::Container container_;
public:
    EnablerGUIComponent(std::function<void(ConfigureCommand &&)> const &configureFeedFunc) 
        : ftxui::Component(), configureFeedFunc_(configureFeedFunc)
        , displayText_(L"Unknown")
        , enableBtn_(L"Enable")
        , disableBtn_(L"Disable")
        , container_(ftxui::Container::Horizontal())
    {
        enableBtn_.on_click = [this]() {
            ConfigureCommand cmd;
            cmd.set_enabled(true);
            configureFeedFunc_(std::move(cmd));
        };
        disableBtn_.on_click = [this]() {
            ConfigureCommand cmd;
            cmd.set_enabled(false);
            configureFeedFunc_(std::move(cmd));
        };
        container_.Add(&enableBtn_);
        container_.Add(&disableBtn_);
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
            }) | ftxui::hcenter
        });
    }
};

int main(int argc, char **argv) {
    TheEnvironment env;
    R r(&env);

    auto configureImporterAndFunc = M::triggerImporter<ConfigureCommand>();
    auto configureImporter = std::get<0>(configureImporterAndFunc);
    r.registerImporter("configureImporter", configureImporter);
    auto configureFeedFunc = std::get<1>(configureImporterAndFunc);

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    EnablerGUIComponent component(configureFeedFunc);

    auto statusHandler = M::pureExporter<bool>(
        [&component,&screen](bool &&data) {
            component.setEnabled(data);
            screen.PostEvent(ftxui::Event::Custom);
        }
    );

    r.registerExporter("statusHandler", statusHandler);

    enablerGUIDataFlow(
        r
        , "ftxui_enabler"
        , r.sourceAsSourceoid(r.importItem(configureImporter))
        , r.sinkAsSinkoid(r.exporterAsSink(statusHandler))
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_enabler");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    component.TakeFocus();
    screen.Loop(&component);
    return 0;
}