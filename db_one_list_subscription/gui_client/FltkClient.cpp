#include "GuiClientDataFlow.hpp"

#ifdef _MSC_VER
#define WIN32
#endif

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

class MyTable : public Fl_Table {
private:
    int64_t version_;
    std::vector<basic::transaction::named_value_store::Item<db_data>> dataVec_;
    basic::transaction::named_value_store::Collection<db_data> dataMap_;
    mutable std::mutex mutex_;

    void DrawHeader(const char *s, int X, int Y, int W, int H) {
        fl_push_clip(X,Y,W,H);
            fl_draw_box(FL_THIN_UP_BOX, X,Y,W,H, row_header_color());
            fl_color(FL_BLACK);
            fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
        fl_pop_clip();
    } 
    void DrawData(const char *s, int X, int Y, int W, int H) {
        fl_push_clip(X,Y,W,H);
            fl_color(FL_WHITE); fl_rectf(X,Y,W,H);
            fl_color(FL_GRAY0); fl_draw(s, X,Y,W,H, FL_ALIGN_CENTER);
            fl_color(color()); fl_rect(X,Y,W,H);
        fl_pop_clip();
    } 

    void draw_cell(TableContext context, int ROW=0, int COL=0, int X=0, int Y=0, int W=0, int H=0) override final {
        static char s[1024];
        switch ( context ) {
        case CONTEXT_STARTPAGE:                   
            fl_font(FL_HELVETICA, 16);   
            return; 
        case CONTEXT_COL_HEADER:   
            switch (COL) {
            case 0:
                DrawHeader("Name", X, Y, W, H);
                break;
            case 1:
                DrawHeader("Amount", X, Y, W, H);
                break;
            case 2:
                DrawHeader("Stat", X, Y, W, H);
                break;
            default:
                break;
            } 
            return; 
        case CONTEXT_ROW_HEADER:  
            return;
        case CONTEXT_CELL:  
            {
                basic::transaction::named_value_store::Item<db_data> thisData;
                {
                    std::lock_guard<std::mutex> _(mutex_);
                    if (static_cast<size_t>(ROW) >= dataVec_.size()) {
                        return;
                    }
                    thisData = dataVec_[ROW];
                }
                switch (COL) {
                case 0:
                    sprintf(s, "%s", std::get<0>(thisData).c_str());
                    DrawData(s, X, Y, W, H);
                    break;
                case 1:
                    sprintf(s, "%d", std::get<1>(thisData).amount);
                    DrawData(s, X, Y, W, H);
                    break;
                case 2:
                    sprintf(s, "%lf", std::get<1>(thisData).stat);
                    DrawData(s, X, Y, W, H);
                    break;
                default:
                    break;
                }
            }
            return;
        default:
            return;
        }
    }
public:
    MyTable(int X, int Y, int W, int H) : Fl_Table(X, Y, W, H), version_(-1), dataVec_(), dataMap_(), mutex_() {
        rows(0);
        row_header(0);
        row_height_all(20);
        row_resize(0);
        cols(3);
        col_header(1);
        col_width_all(200);
        col_resize(1);
        end();
    }
    ~MyTable() {}
    void updateData(DI::FullUpdate &&update) {
        std::lock_guard<std::mutex> _(mutex_);
        if (update.data.empty()) {
            version_ = -1;
            dataVec_.clear();
            dataMap_.clear();
            rows(0);
            redraw();
            return;
        }
        version_ = update.data[0].version;
        dataVec_.clear();
        dataMap_.clear();
        if (update.data[0].data) {
            for (auto const &item : *(update.data[0].data)) {
                dataVec_.push_back({item.first, item.second});
            }
            dataMap_ = std::move(*(update.data[0].data));
        }
        rows(dataMap_.size());
        redraw();
    }
    std::tuple<int64_t, size_t> details() const {
        std::lock_guard<std::mutex> _(mutex_);
        return {version_, dataMap_.size()};
    }
};

std::function<void(TI::Transaction &&)> transactionFunc;
std::function<void()> exitFunc;
Fl_Input *nameInput = nullptr;
Fl_Input *amountInput = nullptr;
Fl_Input *statInput = nullptr;
MyTable *table = nullptr;

void insertUpdateCallback(Fl_Widget *) {
    std::string name = nameInput->value();
    boost::trim(name);
    std::string amount = amountInput->value();
    boost::trim(amount);
    std::string stat = statInput->value();
    boost::trim(stat);
    if (name == "" || amount == "" || stat == "") {
        return;
    }

    basic::transaction::named_value_store::Item<db_data> item;
    try {
        std::get<0>(item) = name;
        std::get<1>(item).amount = boost::lexical_cast<int32_t>(amount);
        std::get<1>(item).stat = boost::lexical_cast<double>(stat);
    } catch (boost::bad_lexical_cast const &) {
        return;
    }

    basic::transaction::named_value_store::CollectionDelta<db_data> delta;
    delta.inserts_updates.push_back(std::move(item));
    auto versionAndOldCount = table->details();
    if (std::get<0>(versionAndOldCount) < 0) {
        return;
    }
    transactionFunc(TI::Transaction { {
        TI::UpdateAction {
            basic::VoidStruct {}, std::get<0>(versionAndOldCount), std::get<1>(versionAndOldCount), delta
        }
    } });
}

void deleteCallback(Fl_Widget *) {
    std::string name = nameInput->value();
    boost::trim(name);
    if (name == "") {
        return;
    }

    basic::transaction::named_value_store::CollectionDelta<db_data> delta;
    delta.deletes.push_back(name);
    auto versionAndOldCount = table->details();
    if (std::get<0>(versionAndOldCount) < 0) {
        return;
    }
    transactionFunc(TI::Transaction { {
        TI::UpdateAction {
            basic::VoidStruct {}, std::get<0>(versionAndOldCount), std::get<1>(versionAndOldCount), delta
        }
    } });
}

void closeCallback(Fl_Widget *w) {
    exitFunc();
}

int main(int argc, char **argv) {
    Fl_Double_Window *window = new Fl_Double_Window(640,580,"FLTK DB One List Client");
    table = new MyTable(20, 20, 602, 300);
    nameInput = new Fl_Input(120, 340, 500, 40, "Name:");
    amountInput = new Fl_Input(120, 400, 500, 40, "Amount:");
    statInput = new Fl_Input(120, 460, 500, 40, "Stat:");
    Fl_Button *insertBtn = new Fl_Button(30, 520, 380, 40, "Insert/Update");
    Fl_Button *deleteBtn = new Fl_Button(430, 520, 180, 40, "Delete");
    window->end();
    Fl::lock();
    window->show(argc, argv);

    TheEnvironment env;
    R r(&env);

    auto transactionImporterAndFunc = M::triggerImporter<TI::Transaction>();
    auto transactionImporter = std::get<0>(transactionImporterAndFunc);
    r.registerImporter("transactionImporter", transactionImporter);
    transactionFunc = std::get<1>(transactionImporterAndFunc);

    auto exitImporterAndFunc = M::constTriggerImporter<GuiExitEvent>();
    auto exitImporter = std::get<0>(exitImporterAndFunc);
    r.registerImporter("exitImporter", exitImporter);
    exitFunc = std::get<1>(exitImporterAndFunc);

    insertBtn->callback(insertUpdateCallback);
    deleteBtn->callback(deleteCallback);
    window->callback(closeCallback);

    auto updateHandler = M::pureExporter<DI::FullUpdate>(
        [](DI::FullUpdate &&data) {
            Fl::lock();
            table->updateData(std::move(data));
            Fl::unlock();
            Fl::awake();
        }
    );
    r.registerExporter("updateHandler", updateHandler);
    auto unsubscribeHandler = M::pureExporter<UnsubscribeConfirmed>(
        [window](UnsubscribeConfirmed &&) {
            Fl::lock();
            window->hide();
            Fl::unlock();
            Fl::awake();
        }
    );
    r.registerExporter("unsubscribeHandler", unsubscribeHandler);

    guiClientDataFlow(
        r
        , "fltk_client"
        , r.sourceAsSourceoid(r.importItem(transactionImporter))
        , r.sourceAsSourceoid(r.importItem(exitImporter))
        , r.sinkAsSinkoid(r.exporterAsSink(updateHandler))
        , r.sinkAsSinkoid(r.exporterAsSink(unsubscribeHandler))
    );

    std::ostringstream graphOss;    
    graphOss << "The graph is:\n";
    r.writeGraphVizDescription(graphOss, "fltk_client");
    r.finalize();

    env.log(infra::LogLevel::Info, graphOss.str());

    int ret = Fl::run();
    env.exit();
    return ret;
}