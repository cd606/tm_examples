#ifndef ENABLEPANEL_HPP
#define ENABLEPANEL_HPP

#include <QWidget>
#include <QLabel>
#include <QPushButton>

class EnablePanel : public QWidget
{
    Q_OBJECT
private:
    QLabel *statusLabel_;
    QPushButton *enableButton_;
    QPushButton *disableButton_;
    std::function<void(bool &&)> setStatusFunc_;
public:
    EnablePanel();
    ~EnablePanel();
    void connectSetStatusFunc(std::function<void(bool &&)> f);
public slots:
    void updateStatus(bool enabled);
};

#endif // ENABLEPANEL_HPP
