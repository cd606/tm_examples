#include "DataSource.hpp"

#include <thread>
#include <atomic>
#include <ctime>

#include <boost/random.hpp>

class DataSourceImpl {
private:
    DataSourceListener *listener_;
    std::thread th_;
    std::atomic<bool> running_;
    void run() {
        boost::random::mt19937 rng;
        rng.seed(std::time(nullptr));
        boost::random::uniform_real_distribution dist(0.0, 100.0);
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            listener_->onData(DataFromSource {dist(rng)});
        }
    }
public:
    DataSourceImpl() : listener_(), th_(), running_(false) {}
    ~DataSourceImpl() {
        if (running_) {
            running_ = false;
            th_.join();
        }
    }
    void start(DataSourceListener *listener) {
        listener_ = listener;
        running_ = true;
        th_ = std::thread(&DataSourceImpl::run, this);
    }
};

DataSource::DataSource() : impl_(std::make_unique<DataSourceImpl>()) {}
DataSource::~DataSource() {}
void DataSource::start(DataSourceListener *listener) {
    impl_->start(listener);
}