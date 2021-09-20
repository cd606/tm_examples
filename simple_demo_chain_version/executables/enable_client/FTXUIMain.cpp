#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#if defined(_has_include)
    #if _has_include(<ftxui/component/button.hpp>) && _has_include(<ftxui/component/container.hpp>)
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

#include "EnablerGUIDataFlow.hpp"

#if FTXUI_OLD_STYLE_COMPONENT == 1
class EnablerGUIComponent : public ftxui::Component {
#else 
class EnablerGUIComponent : public ftxui::ComponentBase {
#endif
private:
    std::function<void(bool &&)> configureFeedFunc_;
    std::function<void()> exitFunc_;
    std::wstring displayText_;
#if FTXUI_OLD_STYLE_COMPONENT == 1
    ftxui::Button enableBtn_, disableBtn_, exitBtn_;
    ftxui::Container container_;
#else 
    ftxui::Component enableBtn_, disableBtn_, exitBtn_;
    ftxui::Component container_;
#endif
public:
    EnablerGUIComponent(std::function<void(bool &&)> const &configureFeedFunc, std::function<void()> const &exitFunc) 
#if FTXUI_OLD_STYLE_COMPONENT == 1
        : ftxui::Component(), configureFeedFunc_(configureFeedFunc), exitFunc_(exitFunc)
#else
        : ftxui::ComponentBase(), configureFeedFunc_(configureFeedFunc), exitFunc_(exitFunc)
#endif
        , displayText_(L"Unknown")
#if FTXUI_OLD_STYLE_COMPONENT == 1
        , enableBtn_(L"Enable")
        , disableBtn_(L"Disable")
        , exitBtn_(L"Exit")
        , container_(ftxui::Container::Horizontal())
#else
        , enableBtn_()
        , disableBtn_()
        , exitBtn_()
        , container_(ftxui::Container::Horizontal({}))
#endif
    {
#if FTXUI_OLD_STYLE_COMPONENT == 1
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
#else 
        enableBtn_ = ftxui::Button(L"Enable", [this]() {
            configureFeedFunc_(true);
        });
        disableBtn_ = ftxui::Button(L"Disable", [this]() {
            configureFeedFunc_(false);
        });
        exitBtn_ = ftxui::Button(L"Exit", [this]() {
            exitFunc_();
        });
        container_->Add(enableBtn_);
        container_->Add(disableBtn_);
        container_->Add(exitBtn_);
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
                , exitBtn_.Render()
#else
                enableBtn_->Render()
                , disableBtn_->Render()
                , exitBtn_->Render()
#endif
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
#if FTXUI_OLD_STYLE_COMPONENT == 1
    auto *component = new EnablerGUIComponent(configureFeedFunc, exitFeedFunc);
#else 
    std::shared_ptr<EnablerGUIComponent> component(new EnablerGUIComponent(configureFeedFunc, exitFeedFunc));
#endif

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
