#pragma once

#include <queue>
#include <atomic>
#include <vector>
#include <bit>
#include <cassert>

template <class T>
class MPMCBoundedQueue {
public:
    explicit MPMCBoundedQueue(int size) : mask_(size - 1) {
        assert(std::popcount(static_cast<unsigned int>(size)) == 1 &&
               static_cast<unsigned int>(size) > 1);
        ringbuffer_ = std::vector<Element>(size);
        max_size_ = static_cast<unsigned int>(size);
        for (unsigned int i = 0; i < size; ++i) {
            ringbuffer_[i].generation.store(i);
        }
    }

    bool Enqueue(const T& value) {
        unsigned int index = end_.load();
        while (true) {
            unsigned int pos = index & mask_;
            unsigned int generation = ringbuffer_[pos].generation.load();
            if (index == generation) {
                if (end_.compare_exchange_weak(index, index + 1)) {
                    ringbuffer_[pos].value = std::move(value);
                    ringbuffer_[pos].generation.fetch_add(1);
                    return true;
                }
            } else if (index > generation) {
                return false;
            } else {
                index = end_.load();
            }
        }
    }

    bool Dequeue(T& data) {
        unsigned int index = begin_.load();
        while (true) {
            unsigned int pos = index & mask_;
            unsigned int generation = ringbuffer_[pos].generation.load();
            if (index + 1 == generation) {
                if (begin_.compare_exchange_weak(index, index + 1)) {
                    data = std::move(ringbuffer_[pos].value);
                    ringbuffer_[pos].generation.fetch_add(max_size_ - 1);
                    return true;
                }
            } else if (index == generation) {
                return false;
            } else {
                index = begin_.load();
            }
        }
    }

private:
    struct Element {
        std::atomic<unsigned int> generation;
        T value;
    };

    std::vector<Element> ringbuffer_;
    std::atomic<unsigned int> begin_ = 0;
    std::atomic<unsigned int> end_ = 0;
    unsigned int mask_ = 0;
    unsigned int max_size_ = 0;
};
