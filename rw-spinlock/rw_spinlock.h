#pragma once

#include <atomic>

struct RWSpinLock {
    void LockRead() {
        int expected = atomic.load();
        expected = expected - expected % 2;
        while (!atomic.compare_exchange_weak(expected, expected + 2)) {
            expected = expected - expected % 2;
        }
    }

    void UnlockRead() {
        atomic.fetch_add(-2);
    }

    void LockWrite() {
        int expected = 0;
        while (!atomic.compare_exchange_weak(expected, 1)) {
            expected = 0;
        };
    }

    void UnlockWrite() {
        atomic.store(0);
    }
    std::atomic<int> atomic = 0;
};
