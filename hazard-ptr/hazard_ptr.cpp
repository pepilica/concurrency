#include "hazard_ptr.h"
#include <vector>
#include <algorithm>

extern thread_local std::atomic<void*> hazard_ptr{nullptr};

std::mutex scan_lock;

MPSCStack<RetiredPtr> free_list;
std::atomic<int> approximate_free_list_size = 0;

std::mutex threads_lock;
std::unordered_set<ThreadState*> threads;

void ClearNonHazardPointers(MPSCStack<RetiredPtr>::Node* list, std::vector<void*>& hazard) {
    using Node = MPSCStack<RetiredPtr>::Node;
    std::less<const void*> tf;
    auto cmp = [&tf](const void* lhv, const void* rhv) -> bool { return tf(lhv, rhv); };
    std::sort(hazard.begin(), hazard.end(), std::function(cmp));
    Node* cur_node = list;
    while (cur_node) {
        RetiredPtr cur_retired_ptr = std::move(cur_node->value);
        void* cur_ptr = cur_retired_ptr.value;
        if (std::lower_bound(hazard.begin(), hazard.end(), cur_ptr, cmp) == hazard.end()) {
            cur_retired_ptr.deleter();
        } else {
            free_list.Push(cur_retired_ptr);
            approximate_free_list_size.fetch_add(1);
        }
        Node* temp = cur_node;
        cur_node = cur_node->next;
        delete temp;
    }
}

void ScanFreeList() {
    using Node = MPSCStack<RetiredPtr>::Node;
    approximate_free_list_size.store(0);
    std::unique_lock idk(scan_lock, std::defer_lock_t());
    if (!idk.try_lock()) {
        return;
    }
    {
        Node* list = free_list.SnatchContents();
        std::vector<void*> hazard;
        {
            std::unique_lock guard(threads_lock);
            for (const auto& thread : threads) {
                if (auto ptr = thread->GetPtr()->load(); ptr) {
                    hazard.push_back(ptr);
                }
            }
        }
        ClearNonHazardPointers(list, hazard);
    }
}