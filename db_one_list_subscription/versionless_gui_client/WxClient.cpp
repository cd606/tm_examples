#ifdef _MSC_VER
#define WIN32
#ifndef WINVER
#define WINVER 0x0601
#endif
#define __WXMSW__
#define WXUSINGDLL
#define wxUSE_GUI 1
#endif

#include "GuiClientDataFlow.hpp"

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/grid.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

wxDECLARE_EVENT(DATA_UPDATE_EVENT, wxCommandEvent);
wxDEFINE_EVENT(DATA_UPDATE_EVENT, wxCommandEvent);
wxDECLARE_EVENT(UNSUBSCRIBE_CONFIRM_EVENT, wxCommandEvent);
wxDEFINE_EVENT(UNSUBSCRIBE_CONFIRM_EVENT, wxCommandEvent);
enum
{
    ID_Data_Update = wxID_HIGHEST+1,
    ID_Unsubscribe_Confirm = wxID_HIGHEST+2,
    ID_Button_Insert_Update = wxID_HIGHEST+3,
    ID_Button_Delete = wxID_HIGHEST+4
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
    basic::transaction::complex_key_value_store::as_collection::Collection<DBKey,DBData> dataMap_;
    mutable std::mutex mutex_;

    wxTextCtrl *nameInput_;
    wxTextCtrl *amountInput_;
    wxTextCtrl *statInput_;
    wxGrid *dataGrid_;

    std::function<void(TI::Transaction &&)> transactionFunc_;
    std::function<void()> exitFunc_;
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
    void dataCallback(DI::FullUpdate &&update) {
        {
            std::lock_guard<std::mutex> _(mutex_);
            if (update.data.empty()) {
                dataMap_.clear();
            } else {
                dataMap_.clear();
                if (update.data[0].data) {
                    dataMap_ = std::move(*(update.data[0].data));
                }
            }
        }
        wxCommandEvent event(DATA_UPDATE_EVENT, ID_Data_Update);
        event.SetEventObject(this);
        ProcessWindowEvent(event);
    }
    void unsubscribeCallback() {
        wxCommandEvent event(UNSUBSCRIBE_CONFIRM_EVENT, ID_Unsubscribe_Confirm);
        event.SetEventObject(this);
        ProcessWindowEvent(event);
    }
    void setTransactionFunc(std::function<void(TI::Transaction &&)> f) {
        transactionFunc_ = f;
    }
    void setExitFunc(std::function<void()> f) {
        exitFunc_ = f;
    }
private:
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnDataUpdate(wxCommandEvent& event);
    void OnUnsubscribeConfirm(wxCommandEvent& event);
    void OnInsertUpdateButtonClick(wxCommandEvent& event);
    void OnDeleteButtonClick(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(wxID_EXIT,  MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
    EVT_CLOSE(MyFrame::OnClose)
    EVT_COMMAND(ID_Data_Update, DATA_UPDATE_EVENT, MyFrame::OnDataUpdate)
    EVT_COMMAND(ID_Unsubscribe_Confirm, UNSUBSCRIBE_CONFIRM_EVENT, MyFrame::OnUnsubscribeConfirm)
    EVT_BUTTON(ID_Button_Insert_Update, MyFrame::OnInsertUpdateButtonClick)
    EVT_BUTTON(ID_Button_Delete, MyFrame::OnDeleteButtonClick)
wxEND_EVENT_TABLE()
wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( "Wx DB One List Client", wxPoint(50, 50), wxSize(640, 580) );
    frame->Show( true );

    auto transactionImporterAndFunc = M::triggerImporter<TI::Transaction>();
    auto transactionImporter = std::get<0>(transactionImporterAndFunc);
    r_->registerImporter("transactionImporter", transactionImporter);
    frame->setTransactionFunc(std::get<1>(transactionImporterAndFunc));

    auto exitImporterAndFunc = M::constTriggerImporter<GuiExitEvent>();
    auto exitImporter = std::get<0>(exitImporterAndFunc);
    r_->registerImporter("exitImporter", exitImporter);
    frame->setExitFunc(std::get<1>(exitImporterAndFunc));

    auto updateHandler = M::pureExporter<DI::FullUpdate>(
        [frame](DI::FullUpdate &&data) {
            frame->dataCallback(std::move(data));
        }
    );
    r_->registerExporter("updateHandler", updateHandler);
    auto unsubscribeHandler = M::pureExporter<UnsubscribeConfirmed>(
        [frame](UnsubscribeConfirmed &&) {
            frame->unsubscribeCallback();
        }
    );
    r_->registerExporter("unsubscribeHandler", unsubscribeHandler);

    guiClientDataFlow(
        *r_
        , "wx_client"
        , r_->sourceAsSourceoid(r_->importItem(transactionImporter))
        , r_->sourceAsSourceoid(r_->importItem(exitImporter))
        , r_->sinkAsSinkoid(r_->exporterAsSink(updateHandler))
        , r_->sinkAsSinkoid(r_->exporterAsSink(unsubscribeHandler))
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r_->writeGraphVizDescription(graphOss, "fltk_client");
    r_->finalize();

    env_->log(infra::LogLevel::Info, graphOss.str());

    return true;
}
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
        , dataMap_(), mutex_()
        , nameInput_(nullptr), amountInput_(nullptr), statInput_(nullptr), dataGrid_(nullptr)
        , transactionFunc_(), exitFunc_()
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
    dataGrid_ = new wxGrid(this, -1, wxDefaultPosition, wxSize(600,300));
    dataGrid_->CreateGrid(0, 3);
    dataGrid_->SetColLabelValue(0, wxT("Name"));
    dataGrid_->SetColLabelValue(1, wxT("Amount"));
    dataGrid_->SetColLabelValue(2, wxT("Stat"));
    dataGrid_->SetColSize(0, 200);
    dataGrid_->SetColSize(1, 200);
    dataGrid_->SetColSize(2, 200);
    dataGrid_->HideRowLabels();
    sizer->Add(dataGrid_);

    auto *rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxStaticText(this, -1, "Name:", wxDefaultPosition, wxSize(100,40)));
    nameInput_ = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(500,40));
    rSizer->Add(nameInput_, wxSizerFlags().Expand());
    sizer->Add(rSizer);

    rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxStaticText(this, -1, "Amount:", wxDefaultPosition, wxSize(100,40)));
    amountInput_ = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(500,40));
    rSizer->Add(amountInput_, wxSizerFlags().Expand());
    sizer->Add(rSizer);

    rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxStaticText(this, -1, "Stat:", wxDefaultPosition, wxSize(100,40)));
    statInput_ = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(500,40));
    rSizer->Add(statInput_, wxSizerFlags().Expand());
    sizer->Add(rSizer);

    rSizer = new wxBoxSizer(wxHORIZONTAL);
    rSizer->Add(new wxButton(this, ID_Button_Insert_Update, "Insert/Update", wxDefaultPosition, wxSize(400,40)));
    rSizer->Add(new wxButton(this, ID_Button_Delete, "Delete", wxDefaultPosition, wxSize(200,40)));
    sizer->Add(rSizer);

    SetSizerAndFit(sizer);
}
void MyFrame::OnExit(wxCommandEvent& event)
{
    Close( true );
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox( "This is WX version of DB One List Client",
                  "About WX DB One List Client", wxOK | wxICON_INFORMATION );
}
void MyFrame::OnClose(wxCloseEvent& event)
{
    exitFunc_();
}
void MyFrame::OnDataUpdate(wxCommandEvent& event)
{
    std::lock_guard<std::mutex> _(mutex_);
    dataGrid_->ClearGrid();   
    auto r = static_cast<size_t>(dataGrid_->GetNumberRows());
    if (r < dataMap_.size()) {
        dataGrid_->AppendRows(dataMap_.size()-r);
    } else if (r > dataMap_.size()) {
        dataGrid_->DeleteRows(0, r-dataMap_.size());
    }
    int row = 0;
    std::ostringstream oss;
    for (auto const &item : dataMap_) {
        dataGrid_->SetCellValue(row, 0, item.first.name);
        dataGrid_->SetCellValue(row, 1, boost::lexical_cast<std::string>(item.second.amount));
        oss.str("");
        oss << std::fixed << std::setprecision(6) << item.second.stat;
        dataGrid_->SetCellValue(row, 2, oss.str());
        ++row;
    }
}
void MyFrame::OnUnsubscribeConfirm(wxCommandEvent& event)
{
    Destroy();
}
void MyFrame::OnInsertUpdateButtonClick(wxCommandEvent& event)
{
    std::string name = nameInput_->GetValue().ToStdString();
    boost::trim(name);
    std::string amount = amountInput_->GetValue().ToStdString();
    boost::trim(amount);
    std::string stat = statInput_->GetValue().ToStdString();
    boost::trim(stat);
    if (name == "" || amount == "" || stat == "") {
        return;
    }

    basic::transaction::complex_key_value_store::as_collection::Item<DBKey,DBData> item;
    try {
        std::get<0>(item).name = name;
        std::get<1>(item).amount = boost::lexical_cast<int32_t>(amount);
        std::get<1>(item).stat = boost::lexical_cast<double>(stat);
    } catch (boost::bad_lexical_cast const &) {
        return;
    }

    basic::transaction::complex_key_value_store::as_collection::CollectionDelta<DBKey,DBData> delta;
    delta.inserts_updates.push_back(std::move(item));

    {
        std::lock_guard<std::mutex> _(mutex_);
        transactionFunc_(TI::Transaction { {
            TI::UpdateAction {
                basic::VoidStruct {}, basic::ConstType<0> {}, dataMap_.size(), delta
            }
        } });
    }
}
void MyFrame::OnDeleteButtonClick(wxCommandEvent& event)
{
    std::string name = nameInput_->GetValue().ToStdString();
    boost::trim(name);
    if (name == "") {
        return;
    }

    basic::transaction::complex_key_value_store::as_collection::CollectionDelta<DBKey,DBData> delta;
    delta.deletes.push_back(DBKey {name});

    {
        std::lock_guard<std::mutex> _(mutex_);
        transactionFunc_(TI::Transaction { {
            TI::UpdateAction {
                basic::VoidStruct {}, basic::ConstType<0> {}, dataMap_.size(), delta
            }
        } });
    }
}
