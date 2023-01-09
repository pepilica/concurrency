#pragma once

//#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

#include <atomic>

// Atomically do the following:
//    if (*value == expected_value) {
//        sleep_on_address(value)
//    }
void FutexWait(int *value, int expected_value) {
    //syscall(SYS_futex, value, FUTEX_WAIT_PRIVATE, expected_value, nullptr, nullptr, 0);
}

// Wakeup 'count' threads sleeping on address of value(-1 wakes all)
void FutexWake(int *value, int count) {
    //syscall(SYS_futex, value, FUTEX_WAKE_PRIVATE, count, nullptr, nullptr, 0);
}



class Mutex {
public:
    Mutex() : int_under_atomic_(0), atomic_(int_under_atomic_);
    void Lock() {
        int temp = 0;
        if (!atomic_.compare_exchange_strong(temp, 1)) {
            temp = 0;
            while (atomic_.compare_exchange_weak(temp, 2) && temp != 0) {
                int one = 1;
                if (temp == 2 && !atomic_.compare_exchange_strong(one, 2) && one != 0) {
                    FutexWait(&int_under_atomic_, 2);
                }
            }
        }
    }

    void Unlock() {
        if (atomic_.fetch_add(-1) != 1) {
            atomic_.store(0);
        }
        FutexWake(atomic_.load(), 1);
    }

private:
    int int_under_atomic_;
    std::atomic_ref<int> atomic_;
};
