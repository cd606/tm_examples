#ifndef EXTERNAL_DATA_SOURCE_HPP_
#define EXTERNAL_DATA_SOURCE_HPP_

#include <memory>

struct DataFromSource {
    double value;
};

class ExternalDataSourceImpl;

class ExternalDataSourceListener {
public:
    virtual void onData(DataFromSource const &) = 0;
    virtual ~ExternalDataSourceListener() {}
};

class ExternalDataSource {
private:
    std::unique_ptr<ExternalDataSourceImpl> impl_;
public:
    ExternalDataSource();
    ~ExternalDataSource();
    void start(ExternalDataSourceListener *listener);
};

#endif