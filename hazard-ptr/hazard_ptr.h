#pragma once

#include <atomic>
#include <functional>
#include <shared_mutex>
#include <mutex>
#include <vector>
#include <memory>
#include <unordered_set>
#include <optional>
#include <iostream>

template <class T>
class MPSCStack {
public:
    struct Node {
        T value;
        Node* next;
    };

    Node* SnatchContents() {
        return head_.exchange(nullptr);
    }

    void Push(const T& value) {
        Node* old_head = head_.load();
        Node* new_head = new Node{value, old_head};
        do {
            new_head->next = old_head;
        } while (!head_.compare_exchange_weak(old_head, new_head));
    }

    std::optional<T> Pop() {
        Node* old_head = head_.load();
        Node* new_head;
        do {
            if (!old_head) {
                return std::nullopt;
            }
            new_head = old_head->next;
        } while (!head_.compare_exchange_weak(old_head, new_head));
        T ans = std::move(old_head->value);
        delete old_head;
        return ans;
    }

    template <class TFn>
    void DequeueAll(const TFn& cb) {
        std::optional<T> elem = Pop();
        while (elem.has_value()) {
            cb(elem.value());
            elem = Pop();
        }
    }

    ~MPSCStack() {
        std::optional<T> elem = Pop();
        while (elem.has_value()) {
            elem = Pop();
        }
    }

private:
    std::atomic<Node*> head_{nullptr};
};

extern std::mutex scan_lock;

extern thread_local std::atomic<void*> hazard_ptr;

class ThreadState {
    std::atomic<void*>* ptr_;

public:
    ThreadState(std::atomic<void*>* ptr) : ptr_(ptr) {
    }
    const std::atomic<void*>* GetPtr() const {
        return ptr_;
    }
};

struct RetiredPtr {
    template <class T, class Deleter>
    RetiredPtr(T* v, Deleter d) : value(v) {
        deleter = [v, d]() { d(v); };
    }

    void* value;
    std::function<void()> deleter;
};

extern MPSCStack<RetiredPtr> free_list;
extern std::atomic<int> approximate_free_list_size;

extern std::mutex threads_lock;
extern std::unordered_set<ThreadState*> threads;

template <class T>
T* Acquire(std::atomic<T*>* ptr) {
    auto value = ptr->load();
    do {
        hazard_ptr.store(value);
        auto new_value = ptr->load();
        if (new_value == value) {
            return value;
        }
        value = new_value;
    } while (true);
}

inline void Release() {
    hazard_ptr.store(nullptr);
}

void ScanFreeList();

template <class T, class Deleter = std::default_delete<T>>
void Retire(T* value, Deleter deleter = {}) {
    RetiredPtr retired_ptr(value, deleter);
    free_list.Push(retired_ptr);
    approximate_free_list_size.fetch_add(1);
    if (approximate_free_list_size.load() > 1024) {
        ScanFreeList();
    }
}

inline void RegisterThread() {
    ThreadState* us = new ThreadState(&hazard_ptr);
    {
        std::lock_guard<std::mutex> lock_guard(threads_lock);
        threads.insert(us);
    }
}

void ClearNonHazardPointers(MPSCStack<RetiredPtr>::Node* list, std::vector<void*>& hazard);

inline void UnregisterThread() {
    std::lock_guard<std::mutex> lock_guard(threads_lock);
    for (auto ts_iter = threads.begin(); ts_iter != threads.end(); ++ts_iter) {
        auto ts = *ts_iter;
        if (&hazard_ptr == ts->GetPtr()) {
            threads.erase(ts_iter);
            delete ts;
            if (threads.empty()) {
                std::vector<void*> dummy;
                ClearNonHazardPointers(free_list.SnatchContents(), dummy);
            }
            return;
        }
    }
    throw std::logic_error({});
}
