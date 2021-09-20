#include "EnablerGUIDataFlow.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#if defined(__has_include)
    #if __has_include(<ftxui/component/button.hpp>) && __has_include(<ftxui/component/container.hpp>)
        #include <ftxui/component/button.hpp>
        #include <ftxui/component/container.hpp>
        #define FTXUI_OLD_STYLE_COMPONENT 1
    #else
        #include <ftxui/component/component.hpp>
        #define FTXUI_OLD_STYLE_COMPONENT 0
    #endif 
#else 
    #include <ftxui/component/component.hpp>
    #define FTXUI_OLD_STYLE_COMPONENT 0
#endif
#include <ftxui/component/screen_interactive.hpp>

#if FTXUI_OLD_STYLE_COMPONENT == 1
class EnablerGUIComponent : public ftxui::Component {
#else 
class EnablerGUIComponent : public ftxui::ComponentBase {
#endif
private:
    std::function<void(ConfigureCommand &&)> configureFeedFunc_;
    std::wstring displayText_;
#if FTXUI_OLD_STYLE_COMPONENT == 1
    ftxui::Button enableBtn_, disableBtn_;
    ftxui::Container container_;
#else 
    ftxui::Component enableBtn_, disableBtn_;
    ftxui::Component container_;
#endif
public:
    EnablerGUIComponent(std::function<void(ConfigureCommand &&)> const &configureFeedFunc) 
#if FTXUI_OLD_STYLE_COMPONENT == 1
        : ftxui::Component(), configureFeedFunc_(configureFeedFunc)
#else
        : ftxui::ComponentBase(), configureFeedFunc_(configureFeedFunc)
#endif
        , displayText_(L"Unknown")
#if FTXUI_OLD_STYLE_COMPONENT == 1
        , enableBtn_(L"Enable")
        , disableBtn_(L"Disable")
        , container_(ftxui::Container::Horizontal())
#else
        , enableBtn_()
        , disableBtn_()
        , container_(ftxui::Container::Horizontal({}))
#endif
    {
#if FTXUI_OLD_STYLE_COMPONENT == 1
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
#else 
        enableBtn_ = ftxui::Button(L"Enable", [this]() {
            ConfigureCommand cmd;
            cmd.set_enabled(true);
            configureFeedFunc_(std::move(cmd));
        });
        disableBtn_ = ftxui::Button(L"Disable", [this]() {
            ConfigureCommand cmd;
            cmd.set_enabled(false);
            configureFeedFunc_(std::move(cmd));
        });
        container_->Add(enableBtn_);
        container_->Add(disableBtn_);
        Add(container_);
#endif
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
#if FTXUI_OLD_STYLE_COMPONENT == 1
                enableBtn_.Render()
                , disableBtn_.Render()
#else
                enableBtn_->Render()
                , disableBtn_->Render()
#endif
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
#if FTXUI_OLD_STYLE_COMPONENT == 1
    EnablerGUIComponent component(configureFeedFunc);
#else 
    auto component = std::make_shared<EnablerGUIComponent>(configureFeedFunc);
#endif

    auto statusHandler = M::pureExporter<bool>(
#if FTXUI_OLD_STYLE_COMPONENT == 1
        [&component,&screen](bool &&data) {
            component.setEnabled(data);
            screen.PostEvent(ftxui::Event::Custom);
        }
#else 
        [component,&screen](bool &&data) {
            component->setEnabled(data);
            screen.PostEvent(ftxui::Event::Custom);
        }
#endif
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

#if FTXUI_OLD_STYLE_COMPONENT == 1
    component.TakeFocus();
    screen.Loop(&component);
#else
    component->TakeFocus();
    screen.Loop(component);
#endif
    return 0;
}