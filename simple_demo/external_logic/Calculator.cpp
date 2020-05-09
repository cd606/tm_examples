#include "Calculator.hpp"

#include <thread>
#include <atomic>
#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <future>

class CalculatorImpl {
private:
    CalculateResultListener *listener_;
    std::thread th_;
    std::atomic<bool> running_;
    std::list<std::tuple<simple_demo::CalculateCommand,int>> incoming_, processing_;
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
                simple_demo::CalculateResult res;
                res.set_id(std::get<0>(cmd).id());
                if (std::get<1>(cmd) == 1) {
                    res.set_result(std::get<0>(cmd).value()*2.0);
                } else {
                    res.set_result(-1.0);
                }
                listener_->onCalculateResult(res);
            }
            processing_.clear();
        }
    }
public:
    CalculatorImpl() : 
        listener_(nullptr), th_(), running_(false)
        , incoming_(), processing_(), mutex_(), cond_() 
    {}
    ~CalculatorImpl() {
        if (running_) {
            running_ = false;
            th_.join();
        }
    }
    void start(CalculateResultListener *listener) {
        listener_ = listener;
        running_ = true;
        th_ = std::thread(&CalculatorImpl::run, this);
    }
    void request(simple_demo::CalculateCommand const &cmd, int count) {
        if (running_) {
            {
                std::lock_guard<std::mutex> _(mutex_);
                incoming_.push_back({cmd, count});
            }
            cond_.notify_one();
        }    
    }
};

Calculator::Calculator() : impl_(std::make_unique<CalculatorImpl>()) {}
Calculator::~Calculator() {}
void Calculator::start(CalculateResultListener *listener) {
    impl_->start(listener);
}
void Calculator::request(simple_demo::CalculateCommand const &cmd) {
    std::thread([this,cmd]() {
        impl_->request(cmd, 1);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        impl_->request(cmd, 2);
    }).detach();
}