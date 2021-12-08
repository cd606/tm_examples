#ifndef SIGNALOBJECT_H
#define SIGNALOBJECT_H

#include <QObject>

class SignalObject : public QObject
{
    Q_OBJECT
public:
    SignalObject() = default;
signals:
    void valueUpdated(double x);
public:
    void setValue(double x) {
        emit valueUpdated(x);
    }
};

#endif // SIGNALOBJECT_H
