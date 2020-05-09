#ifndef CALCULATOR_HPP_
#define CALCULATOR_HPP_

#include <memory>
#include "defs.pb.h"

class CalculateResultListener {
public:
    virtual void onCalculateResult(simple_demo::CalculateResult const &) = 0;
    virtual ~CalculateResultListener() {}
};

class CalculatorImpl;

class Calculator {
private:
    std::unique_ptr<CalculatorImpl> impl_;
public:
    Calculator();
    ~Calculator();
    void start(CalculateResultListener *listener);
    void request(simple_demo::CalculateCommand const &cmd);
};

#endif