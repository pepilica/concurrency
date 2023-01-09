#pragma once

#include <atomic>
#include <optional>
#include <stdexcept>
#include <utility>

template <class T>
class MPSCStack {
public:
    // Push adds one element to stack top.
    //
    // Safe to call from multiple threads.
    void Push(const T& value) {
        Node* old_head = head_.load();
        Node* new_head = new Node{value, old_head};
        do {
            new_head->next = old_head;
        } while (!head_.compare_exchange_weak(old_head, new_head));
    }

    // Pop removes top element from the stack.
    //
    // Not safe to call concurrently.
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

    // DequeuedAll Pop's all elements from the stack and calls cb() for each.
    //
    // Not safe to call concurrently with Pop()
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
    struct Node {
        T value;
        Node* next;
    };

    struct ConcurrentNode {
        uint32_t counter;
        Node* inside_node;
    };

    std::atomic<Node*> head_{nullptr};
};
