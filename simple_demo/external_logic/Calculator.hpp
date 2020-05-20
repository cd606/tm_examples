#ifndef CALCULATOR_HPP_
#define CALCULATOR_HPP_

#include <memory>

struct CalculatorInput {
    int id;
    double input;
};
struct CalculatorOutput {
    int id;
    double output;
};

class CalculateResultListener {
public:
    virtual void onCalculateResult(CalculatorOutput const &) = 0;
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
    void request(CalculatorInput const &cmd);
};

#endif