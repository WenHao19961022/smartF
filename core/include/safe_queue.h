#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;

public:
    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::move(item));
        lock.unlock();
        cond_.notify_one();
    }

    // 带有超时的出队，非常适合 Core 用来做“定时常态触发”
    bool pop_with_timeout(T& item, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cond_.wait_for(lock, std::chrono::milliseconds(timeout_ms), 
                           [this]() { return !queue_.empty(); })) {
            item = std::move(queue_.front());
            queue_.pop();
            return true;
        }
        return false;
    }
};