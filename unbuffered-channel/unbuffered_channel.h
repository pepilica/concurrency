#pragma once

#include <utility>
#include <optional>
#include <condition_variable>
#include <deque>
#include <semaphore>
#include <stdexcept>
#include <mutex>
#include <iostream>

using namespace std::chrono_literals;

template <class T>
class UnbufferedChannel {
public:
    using Pair = std::pair<std::shared_ptr<bool>, T>;
    void Send(const T& value) {
        std::unique_lock send_lock(guard_);
        if (!is_opened_.load()) {
            receive_cv_.notify_all();
            send_cv_.notify_all();
            throw std::runtime_error("closed");
        }
        auto is_received = std::make_shared<bool>(false);
        queue_.push_back(std::make_pair(is_received, value));
        receive_cv_.notify_one();
        send_cv_.wait(send_lock, [this, &is_received]() { return *is_received; });
        send_cv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock receive_lock(guard_);
        receive_cv_.wait(receive_lock, [this]() { return !queue_.empty() || !is_opened_.load(); });
        if (!is_opened_.load()) {
            if (queue_.empty()) {
                receive_cv_.notify_all();
                send_cv_.notify_all();
                return std::nullopt;
            }
        }
        Pair pair = queue_.front();
        std::optional<T> return_value = pair.second;
        queue_.pop_front();
        *(pair.first) = true;
        if (!is_opened_.load()) {
            send_cv_.notify_all();
            receive_cv_.notify_all();
        } else {
            send_cv_.notify_one();
            receive_cv_.notify_one();
        }
        return return_value;
    }

    void Close() {
        std::unique_lock close_lock(guard_);
        is_opened_.store(false);
        send_cv_.notify_all();
        receive_cv_.notify_all();
    }

private:
    std::atomic<bool> is_opened_{true};
    std::deque<Pair> queue_;
    std::mutex guard_;
    std::condition_variable send_cv_;
    std::condition_variable receive_cv_;
};
