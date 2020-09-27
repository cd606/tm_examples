#ifdef _MSC_VER
#define WIN32
#define WINVER 0x0400
#define __WXMSW__
#define WXUSINGDLL
#define wxUSE_GUI 1
#endif

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "DataDisplayFlow.hpp"

#include <tm_kit/basic/SpdLoggingComponent.hpp>
#include <tm_kit/transport/CrossGuidComponent.hpp>

#include <sstream>
#include <iostream>
#include <mutex>

wxDECLARE_EVENT(LABEL_UPDATE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(LABEL_UPDATE_EVENT, wxCommandEvent);
enum
{
    ID_Label_Update = 1,
};
std::atomic<bool> running { true };

using TheEnvironment = infra::Environment<
    infra::CheckTimeComponent<false>,
    infra::TrivialExitControlComponent,
    transport::CrossGuidComponent,
    basic::TimeComponentEnhancedWithSpdLogging<basic::real_time_clock::ClockComponent>,
    transport::AllNetworkTransportComponents
>;
using M = infra::RealTimeApp<TheEnvironment>;
using R = infra::AppRunner<M>;

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
    wxStaticText *label_;
    double value_;
    std::mutex mutex_;
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    void dataCallback(simple_demo::InputData &&d) {
        {
            std::lock_guard<std::mutex> _(mutex_);
            value_ = d.value();
        }
        wxCommandEvent event(LABEL_UPDATE_EVENT, ID_Label_Update);
        event.SetEventObject(this);
        ProcessWindowEvent(event);
    }
private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnLabelUpdate(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT,  MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_CLOSE(MyFrame::OnClose)
    EVT_COMMAND(ID_Label_Update, LABEL_UPDATE_EVENT, MyFrame::OnLabelUpdate)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( "Wx Input Data Display", wxPoint(50, 50), wxSize(450, 340) );
    frame->Show( true );

    env_->setLogFilePrefix("wx_display", true);

    auto dataPrinter = M::pureExporter<simple_demo::InputData>(
        [frame](simple_demo::InputData &&d) {
            if (running) {
                frame->dataCallback(std::move(d));
            }
        }
    );
    r_->registerExporter("dataPrinter", dataPrinter);
    dataDisplayFlow<TheEnvironment>(
        *r_
        , r_->sinkAsSinkoid(r_->exporterAsSink(dataPrinter))
    );

    r_->finalize();

    return true;
}
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size), label_(nullptr), value_(0), mutex_()
{
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, "&File" );
    menuBar->Append( menuHelp, "&Help" );
    SetMenuBar( menuBar );

    label_ = new wxStaticText(this, -1, "Value:");
    label_->SetWindowStyle(wxST_NO_AUTORESIZE);
    label_->CenterOnParent();
}
void MyFrame::OnExit(wxCommandEvent& event)
{
    running = false;
    Close( true );
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox( "This is WX version of input data display",
                  "About WX Input Data Display", wxOK | wxICON_INFORMATION );
}
void MyFrame::OnClose(wxCloseEvent& event)
{
    running = false;
    Destroy();
}
void MyFrame::OnLabelUpdate(wxCommandEvent& event)
{
    std::ostringstream oss;
    {
        std::lock_guard<std::mutex> _(mutex_);
        oss << "Value: " << std::fixed << std::setprecision(6) << value_;
    }
    label_->SetLabel(oss.str());
}
