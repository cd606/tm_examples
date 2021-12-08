#ifndef LINESERIESHPP_H
#define LINESERIESHPP_H

#include <QLineSeries>
#include <QChart>
#include <QChartView>

class LineSeries : public QLineSeries
{
    Q_OBJECT
private:
    unsigned counter_ = 0;
    QChart *chart_;
    QChartView *chartView_;

public:
    LineSeries();
    ~LineSeries();

    QChartView *chartView() const {
        return chartView_;
    }

public slots:
    void addValue(double x);
};

#endif // LINESERIESHPP_H
