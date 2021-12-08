#include <QApplication>

#include "mainwindow.h"
#include "tmPart.hpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    LineSeries *ls = new LineSeries();
    w.setCentralWidget(ls->chartView());

    tm_part::setup(ls);

    w.show();
    return a.exec();
}
