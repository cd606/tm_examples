#ifdef _MSC_VER
#define WIN32
#define WINVER 0x0400
#define __WXMSW__
#define WXUSINGDLL
#define wxUSE_GUI 1
#endif

#include "EnablerGUIDataFlow.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

wxDECLARE_EVENT(DATA_UPDATE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(DATA_UPDATE_EVENT, wxCommandEvent);
enum
{
    ID_Data_Update = wxID_HIGHEST+1,
    ID_Button_Enable = wxID_HIGHEST+2,
    ID_Button_Disable = wxID_HIGHEST+3
};

class MyApp: public wxApp
{
private:
    TheEnvironment *env_;
    R *r_;
public:
    MyApp() : wxApp(), env_(new TheEnvironment {}), r_(new R {env_}) {}
    virtual bool OnInit();
};
class MyFrame: public wxFrame
{
private:
    std::atomic<bool> enabled_;

    wxStaticText *status_;
    wxButton *enableBtn_;
    wxButton *disableBtn_;

    std::function<void(bool &&)> configureFeedFunc_;
    std::function<void()> exitFeedFunc_;
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    void statusCallback(bool enabled) {
        enabled_ = enabled;
        wxCommandEvent event(DATA_UPDATE_EVENT, ID_Data_Update);
        event.SetEventObject(this);
        ProcessWindowEvent(event);
    }
    void setConfigureFeedFunc(std::function<void(bool &&)> const &f) {
        configureFeedFunc_ = f;
    }
    void setExitFeedFunc(std::function<void()> const &f) {
        exitFeedFunc_ = f;
    }
private:
    void OnClose(wxCloseEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnDataUpdate(wxCommandEvent& event);
    void OnEnableButtonClick(wxCommandEvent& event);
    void OnDisableButtonClick(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT,  MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_COMMAND(ID_Data_Update, DATA_UPDATE_EVENT, MyFrame::OnDataUpdate)
    EVT_BUTTON(ID_Button_Enable, MyFrame::OnEnableButtonClick)
    EVT_BUTTON(ID_Button_Disable, MyFrame::OnDisableButtonClick)
    EVT_CLOSE(MyFrame::OnClose)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( "Wx Enabler", wxPoint(50, 50), wxSize(300, 100) );
    frame->Show( true );
    env_->CustomizedExitControlComponent::operator=(
        CustomizedExitControlComponent(
            [frame]() {
                frame->Destroy();
            }
        )
    );

    auto configureImporterAndFunc = M::triggerImporter<bool>();
    auto configureImporter = std::get<0>(configureImporterAndFunc);
    r_->registerImporter("configureImporter", configureImporter);
    frame->setConfigureFeedFunc(std::get<1>(configureImporterAndFunc));

    auto exitImporterAndFunc = M::constTriggerImporter<basic::VoidStruct>();
    auto exitImporter = std::get<0>(exitImporterAndFunc);
    r_->registerImporter("exitImporter", exitImporter);
    frame->setExitFeedFunc(std::get<1>(exitImporterAndFunc));

    auto statusHandler = M::pureExporter<bool>(
        [frame](bool &&data) {
            frame->statusCallback(data);
        }
    );
    r_->registerExporter("statusHandler", statusHandler);

    enablerGUIDataFlow(
        *r_
        , "wx_enabler"
        , r_->sourceAsSourceoid(r_->importItem(configureImporter))
        , r_->sinkAsSinkoid(r_->exporterAsSink(statusHandler))
        , {r_->importItem(exitImporter)}
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r_->writeGraphVizDescription(graphOss, "wx_enabler");
    r_->finalize();

    env_->log(infra::LogLevel::Info, graphOss.str());

    return true;
}
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
        , enabled_(false)
        , status_(nullptr), enableBtn_(nullptr), disableBtn_(nullptr)
        , configureFeedFunc_()
{
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, "&File" );
    menuBar->Append( menuHelp, "&Help" );
    SetMenuBar( menuBar );

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    status_ = new wxStaticText(this, -1, "", wxDefaultPosition, wxSize(100,20));
    status_->SetWindowStyle(wxST_NO_AUTORESIZE);
    sizer->Add(status_);

    auto *rSizer = new wxBoxSizer(wxHORIZONTAL);
    enableBtn_ = new wxButton(this, ID_Button_Enable, "Enable", wxDefaultPosition, wxSize(120, 30));
    disableBtn_ = new wxButton(this, ID_Button_Disable, "Disable", wxDefaultPosition, wxSize(120, 30));
    enableBtn_->Enable(false);
    disableBtn_->Enable(false);
    rSizer->Add(enableBtn_);
    rSizer->Add(disableBtn_);
    sizer->Add(rSizer);

    SetSizerAndFit(sizer);
}
void MyFrame::OnClose(wxCloseEvent& event)
{
    exitFeedFunc_();
}
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close( false );
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox( "This is WX version of Enabler Client",
                  "About WX Enable Client", wxOK | wxICON_INFORMATION );
}
void MyFrame::OnDataUpdate(wxCommandEvent& event)
{
    if (enabled_) {
        status_->SetLabel("Enabled");
        enableBtn_->Enable(false);
        disableBtn_->Enable(true);
    } else {
        status_->SetLabel("Disabled");
        enableBtn_->Enable(true);
        disableBtn_->Enable(false);
    }
}
void MyFrame::OnEnableButtonClick(wxCommandEvent& event)
{
    configureFeedFunc_(true);
}
void MyFrame::OnDisableButtonClick(wxCommandEvent& event)
{
    configureFeedFunc_(false);
}
