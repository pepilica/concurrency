#pragma once

#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <list>
#include <memory>
#include <exception>
#include <deque>
#include <iostream>
#include <atomic>

template <class K, class V, class Hash = std::hash<K>>
class ConcurrentHashMap {
    using Pair = std::pair<K, V>;

public:
    ConcurrentHashMap(const Hash& hasher = Hash()) : ConcurrentHashMap(kUndefinedSize, hasher) {
    }

    explicit ConcurrentHashMap(int expected_size, const Hash& hasher = Hash())
        : ConcurrentHashMap(expected_size, kDefaultConcurrencyLevel, hasher) {
    }

    ConcurrentHashMap(int expected_size, int expected_threads_count, const Hash& hasher = Hash())
        :  // 0 because unordered_map doesn't contain constructor with hasher
          hasher_(hasher),
          buckets_sz_(),
          thread_size_(expected_threads_count),
          mutexes_(),
          buckets_(),
          max_bucket_sz_(0),
          counter_(0) {

        if (expected_size != kUndefinedSize) {
            buckets_.resize(expected_size);
            mutexes_.resize(expected_size);
            buckets_sz_ = expected_size;
        } else {
            buckets_sz_ = std::max(expected_threads_count, 32);
            buckets_.resize(buckets_sz_);
            mutexes_.resize(buckets_sz_);
        }
    }

    bool Insert(const K& key, const V& value) {
        if (10 * counter_.load() >= 9 * buckets_sz_.load()) {
            Rehash();
        }
        {
            std::lock_guard<std::mutex> lock(mutexes_[GetMutexNum(key)]);
            size_t pos = GetBucket(key);
            std::vector<Pair>& list = buckets_[GetBucket(key)];
            for (auto [k, v] : list) {
                if (k == key) {
                    return false;
                }
            }
            list.push_back({key, value});
            counter_.fetch_add(1);
            max_bucket_sz_.store(std::max(buckets_[pos].size(), max_bucket_sz_.load()));
        }
        return true;
    }

    bool Erase(const K& key) {
        {
            std::lock_guard<std::mutex> lock(mutexes_[GetMutexNum(key)]);
            std::vector<Pair>& list = buckets_[GetBucket(key)];
            for (size_t i = 0; i < list.size(); ++i) {
                if (list[i].first == key) {
                    std::swap(list[i], list.back());
                    list.pop_back();
                    counter_.fetch_add(-1);
                    return true;
                }
            }
        }
        return false;
    }

    void Clear() {
        for (size_t i = 0; i < thread_size_; ++i) {
            mutexes_[i].lock();
        }
        counter_.store(0);
        buckets_ = std::vector<std::vector<Pair>>(buckets_sz_);
        for (size_t i = 0; i < thread_size_; ++i) {
            mutexes_[thread_size_ - 1 - i].unlock();
        }
    }

    std::pair<bool, V> Find(const K& key) const {
        {
            std::lock_guard<std::mutex> lock(mutexes_[GetMutexNum(key)]);
            size_t pos = GetBucket(key);
            const std::vector<Pair>& list = buckets_[pos];
            for (auto [k, v] : list) {
                if (k == key) {
                    return std::make_pair(true, v);
                }
            }
        }
        return std::make_pair(false, V());
    }

    const V At(const K& key) const {
        {
            std::lock_guard<std::mutex> lock(mutexes_[GetMutexNum(key)]);
            size_t pos = GetBucket(key);
            const std::vector<Pair>& list = buckets_[pos];
            for (auto [k, v] : list) {
                if (k == key) {
                    return v;
                }
            }
        }
        throw std::out_of_range(" ");
    }

    size_t Size() const {
        return counter_.load();
    }

    static const int kDefaultConcurrencyLevel;
    static const int kUndefinedSize;

private:
    mutable std::vector<std::vector<Pair>> buckets_;
    mutable std::atomic<size_t> buckets_sz_;
    const size_t thread_size_;
    std::atomic<size_t> max_bucket_sz_;
    std::atomic<size_t> counter_;
    mutable std::deque<std::mutex> mutexes_;
    void Rehash() const {
        for (size_t i = 0; i < thread_size_; ++i) {
            mutexes_[i].lock();
        }
        buckets_sz_.store(std::max(1ul, 2 * buckets_sz_.load()));
        std::vector<std::vector<Pair>> new_buckets(buckets_sz_.load());
        for (auto iter = buckets_.begin(); iter != buckets_.end(); ++iter) {
            for (auto iter_list = iter->begin(); iter_list != iter->end(); ++iter_list) {
                auto& elem = *iter_list;
                size_t hash = hasher_(elem.first) % buckets_sz_;
                new_buckets[hash].push_back(elem);
            }
        }
        buckets_ = std::move(new_buckets);
        for (size_t i = 0; i < thread_size_; ++i) {
            mutexes_[thread_size_ - 1 - i].unlock();
        }
    };

    size_t GetBucket(K key) const {
        return hasher_(key) % buckets_sz_;
    };

    size_t GetMutexNum(K key) const {
        return GetBucket(key) % thread_size_;
    }

    Hash hasher_;
};

template <class K, class V, class Hash>
const int ConcurrentHashMap<K, V, Hash>::kDefaultConcurrencyLevel = 8;

template <class K, class V, class Hash>
const int ConcurrentHashMap<K, V, Hash>::kUndefinedSize = -1;
