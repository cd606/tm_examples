#ifndef DATA_SOURCE_HPP_
#define DATA_SOURCE_HPP_

#include <memory>

struct DataFromSource {
    double value;
};

class DataSourceImpl;

class DataSourceListener {
public:
    virtual void onData(DataFromSource const &) = 0;
    virtual ~DataSourceListener() {}
};

class DataSource {
private:
    std::unique_ptr<DataSourceImpl> impl_;
public:
    DataSource();
    ~DataSource();
    void start(DataSourceListener *listener);
};

#endif