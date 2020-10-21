#include "ExternalCalculator.hpp"

#include <thread>
#include <atomic>
#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>

class ExternalCalculatorImpl {
private:
    CalculateResultListener *listener_;
    std::thread th_;
    std::atomic<bool> running_;
    std::list<std::tuple<ExternalCalculatorInput,int>> incoming_, processing_;
    std::mutex mutex_;
    std::condition_variable cond_;
    void run() {
        while (running_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait_for(lock, std::chrono::seconds(1));
                if (!running_) {
                    lock.unlock();
                    return;
                }
                if (incoming_.empty()) {
                    lock.unlock();
                    continue;
                }
                processing_.splice(processing_.end(), incoming_);
                lock.unlock();
            }
            for (auto const &cmd : processing_) {
                ExternalCalculatorOutput res;
                res.id = std::get<0>(cmd).id;
                if (std::get<1>(cmd) == 1) {
                    res.output = std::get<0>(cmd).input*2.0;
                } else {
                    res.output = -1.0;
                }
                listener_->onCalculateResult(res);
            }
            processing_.clear();
        }
    }
public:
    ExternalCalculatorImpl() : 
        listener_(nullptr), th_(), running_(false)
        , incoming_(), processing_(), mutex_(), cond_() 
    {}
    ~ExternalCalculatorImpl() {
        if (running_) {
            running_ = false;
            th_.join();
        }
    }
    void start(CalculateResultListener *listener) {
        listener_ = listener;
        running_ = true;
        th_ = std::thread(&ExternalCalculatorImpl::run, this);
    }
    void request(ExternalCalculatorInput const &cmd, int count) {
        if (running_) {
            {
                std::lock_guard<std::mutex> _(mutex_);
                incoming_.push_back({cmd, count});
            }
            cond_.notify_one();
        }    
    }
};

ExternalCalculator::ExternalCalculator() : impl_(std::make_unique<ExternalCalculatorImpl>()) {}
ExternalCalculator::~ExternalCalculator() {}
void ExternalCalculator::start(CalculateResultListener *listener) {
    impl_->start(listener);
}
void ExternalCalculator::request(ExternalCalculatorInput const &cmd) {
    std::thread([this,cmd]() {
        impl_->request(cmd, 1);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        impl_->request(cmd, 2);
    }).detach();
}