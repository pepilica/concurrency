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
class BufferedChannel {
public:
    explicit BufferedChannel(int size) : size_(0), n_(size), is_opened_(true) {
    }

    void Send(const T& value) {
        std::unique_lock send_lock(guard_);
        send_cv_.wait(send_lock, [this]() { return size_ < n_ || !is_opened_.load(); });
        if (!is_opened_.load()) {
            receive_cv_.notify_all();
            send_cv_.notify_all();
            throw std::runtime_error("closed");
        }
        ++size_;
        queue_.push_back(value);
        receive_cv_.notify_one();
        send_cv_.notify_one();
    }

    std::optional<T> Recv() {
        std::unique_lock receive_lock(guard_);
        receive_cv_.wait(receive_lock, [this]() { return size_ > 0 || !is_opened_.load(); });
        if (!is_opened_.load()) {
            if (size_ == 0) {
                receive_cv_.notify_all();
                send_cv_.notify_all();
                return std::nullopt;
            }
        }
        --size_;
        std::optional<T> return_value = queue_.front();
        queue_.pop_front();
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
    int size_;
    const int n_;
    std::atomic<bool> is_opened_;
    std::deque<T> queue_;
    std::mutex guard_;
    std::condition_variable send_cv_;
    std::condition_variable receive_cv_;
};
