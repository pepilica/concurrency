#pragma once

#include <mutex>
#include <vector>
#include <condition_variable>

using namespace std::chrono_literals;

class DefaultCallback {
public:
    void operator()(int& value) {
        --value;
    }
};

class Semaphore {
public:
    Semaphore(int count) : count_(count) {
    }

    void Leave() {
        std::unique_lock<std::mutex> lock(mutex_);
        ++count_;
        cv_.notify_all();
    }

    template <class Func>
    void Enter(Func callback) {
        std::unique_lock<std::mutex> lock(mutex_);
        ++waiting_count_;
        int my_position = waiting_count_;
        while (!count_ && my_position != 1) {
            cv_.wait_for(lock, 10ms);
        }
        --waiting_count_;
        callback(count_);
    }

    void Enter() {
        DefaultCallback callback;
        Enter(callback);
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_ = 0;
    int waiting_count_ = 0;
};
