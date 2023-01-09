#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>
#include <queue>
#include <iostream>

using namespace std::chrono_literals;

template <class T>
class TimerQueue {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;
    using Pair = std::pair<TimePoint, T>;

public:
    void Add(const T& item, TimePoint at) {
        // Your code goes here
        bool needed2notify = false;
        std::unique_lock lock(mutex_);
        if (!timepts_.empty()) {
            Pair top = timepts_.top();
            if (top.first > at) {
                needed2notify = true;
            }
            timepts_.push({at, item});
            lock.unlock();
            if (needed2notify) {
                var_.notify_all();
            }
        } else {
            timepts_.push({at, item});
            lock.unlock();
        }
    }

    T Pop() {
        bool time_has_come = false;
        T ans;
        while (!time_has_come) {
            std::unique_lock lock(mutex_);
            if (!timepts_.empty()) {
                Pair top = timepts_.top();
                if (Clock::now() >= top.first) {
                    time_has_come = true;
                    ans = top.second;
                    timepts_.pop();
                }
                var_.wait_until(lock, top.first);
            } else {
                var_.wait_for(lock, 10ms);
            }
        }
        return ans;
    }

private:
    std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> timepts_;
    std::condition_variable var_;
    std::mutex mutex_;
};
