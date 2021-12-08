#include "LineSeries.hpp"
#include <iostream>

LineSeries::LineSeries()
    : QLineSeries()
    , chart_(new QChart())
    , chartView_(new QChartView(chart_))
{
    chart_->legend()->hide();
    chart_->addSeries(this);
    chart_->createDefaultAxes();
    chart_->setTitle("Test Chart");
    auto ax = chart_->axes(Qt::Vertical);
    ax.back()->setRange(0, 100);

    chartView_->setRenderHint(QPainter::Antialiasing);
}

LineSeries::~LineSeries()
{
}

void LineSeries::addValue(double x)
{
    append(counter_++, x);
    auto ax = chart_->axes(Qt::Horizontal);
    ax.back()->setRange(0, counter_);
    chart_->setTitle(QString::fromStdString("Value="+std::to_string(x)));
}
