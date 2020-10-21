#include "ExternalDataSource.hpp"

#include <thread>
#include <atomic>
#include <ctime>

#include <boost/random.hpp>

class ExternalDataSourceImpl {
private:
    ExternalDataSourceListener *listener_;
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
    ExternalDataSourceImpl() : listener_(), th_(), running_(false) {}
    ~ExternalDataSourceImpl() {
        if (running_) {
            running_ = false;
            th_.join();
        }
    }
    void start(ExternalDataSourceListener *listener) {
        listener_ = listener;
        running_ = true;
        th_ = std::thread(&ExternalDataSourceImpl::run, this);
    }
};

ExternalDataSource::ExternalDataSource() : impl_(std::make_unique<ExternalDataSourceImpl>()) {}
ExternalDataSource::~ExternalDataSource() {}
void ExternalDataSource::start(ExternalDataSourceListener *listener) {
    impl_->start(listener);
}