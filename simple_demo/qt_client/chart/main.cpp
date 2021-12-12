#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QLayout>

#include "mainwindow.h"
#include "LineSeries.hpp"
#include "EnablePanel.hpp"
#include "tmPart.hpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    LineSeries *ls = new LineSeries();
    EnablePanel *ep = new EnablePanel();

    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(ls->chartView());
    layout->addWidget(ep);

    QWidget *wnd = new QWidget();
    wnd->setLayout(layout);
    w.setCentralWidget(wnd);

    bool secureServers = (argc > 1 && std::string_view(argv[1]) == "secure");
    tm_part::setup(secureServers, ls, ep);

    w.show();
    return a.exec();
}
