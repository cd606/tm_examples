#ifndef EXTERNAL_CALCULATOR_HPP_
#define EXTERNAL_CALCULATOR_HPP_

#include <memory>

struct ExternalCalculatorInput {
    int id;
    double input;
};
struct ExternalCalculatorOutput {
    int id;
    double output;
};

class CalculateResultListener {
public:
    virtual void onCalculateResult(ExternalCalculatorOutput const &) = 0;
    virtual ~CalculateResultListener() {}
};

class ExternalCalculatorImpl;

class ExternalCalculator {
private:
    std::unique_ptr<ExternalCalculatorImpl> impl_;
public:
    ExternalCalculator();
    ~ExternalCalculator();
    void start(CalculateResultListener *listener);
    void request(ExternalCalculatorInput const &cmd);
};

#endif