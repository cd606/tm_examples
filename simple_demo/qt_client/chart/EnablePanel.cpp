#include "EnablePanel.hpp"
#include <QLayout>

EnablePanel::EnablePanel()
    : QWidget()
    , statusLabel_(new QLabel("Unknown"))
    , enableButton_(new QPushButton("Enable"))
    , disableButton_(new QPushButton("Disable"))
    , setStatusFunc_()
{
    auto *l = new QHBoxLayout;
    l->addWidget(statusLabel_);
    l->addWidget(enableButton_);
    l->addWidget(disableButton_);
    setLayout(l);

    enableButton_->setEnabled(false);
    disableButton_->setEnabled(false);

    QObject::connect(enableButton_, &QPushButton::released, this, [this]() {
       if (setStatusFunc_) {
           setStatusFunc_(true);
       }
    });
    QObject::connect(disableButton_, &QPushButton::released, this, [this]() {
       if (setStatusFunc_) {
           setStatusFunc_(false);
       }
    });
}

EnablePanel::~EnablePanel() {}

void EnablePanel::connectSetStatusFunc(std::function<void(bool &&)> f)
{
    setStatusFunc_ = f;
}

void EnablePanel::updateStatus(bool enabled)
{
    if (enabled)
    {
        statusLabel_->setText("Enabled");
        enableButton_->setEnabled(false);
        disableButton_->setEnabled(true);
    }
    else
    {
        statusLabel_->setText("Disabled");
        enableButton_->setEnabled(true);
        disableButton_->setEnabled(false);
    }
}
