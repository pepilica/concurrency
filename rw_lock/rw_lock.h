#pragma once
#include <mutex>
#include <semaphore>

class RWLock {

public:
    template <class Func>
    void Read(Func func) {
        read_.acquire();
        ++blocked_readers_;
        if (blocked_readers_ == 1) {
            global_.acquire();
        }
        read_.release();
        try {
            func();
        } catch (...) {
            EndRead();
            throw;
        }
        EndRead();
    }

    template <class Func>
    void Write(Func func) {
        global_.acquire();
        try {
            func();
        } catch (...) {
            global_.release();
            throw;
        }
        global_.release();
    }

private:
    std::binary_semaphore read_{1};
    std::binary_semaphore global_{1};
    int blocked_readers_ = 0;

    void EndRead() {
        read_.acquire();
        --blocked_readers_;
        if (!blocked_readers_) {
            global_.release();
        }
        read_.release();
    }
};
