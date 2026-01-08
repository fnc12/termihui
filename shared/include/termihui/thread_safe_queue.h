#pragma once

#include <mutex>
#include <vector>
#include <optional>

namespace termihui {

/**
 * Thread-safe queue for strings
 * All operations are protected by mutex
 */
template<typename T>
class ThreadSafeQueue {
public:
    /**
     * Push item to back of queue
     */
    void push(T item) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->items_.push_back(std::move(item));
    }
    
    /**
     * Pop item from front of queue
     * @return item or nullopt if empty
     */
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        if (this->items_.empty()) {
            return std::nullopt;
        }
        T item = std::move(this->items_.front());
        this->items_.erase(this->items_.begin());
        return item;
    }
    
    /**
     * Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(this->mutex_);
        return this->items_.empty();
    }
    
    /**
     * Get number of items in queue
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(this->mutex_);
        return this->items_.size();
    }
    
    /**
     * Clear all items from queue
     */
    void clear() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->items_.clear();
    }
    
    /**
     * Take all items from queue at once
     * @return all items, queue is cleared
     */
    std::vector<T> takeAll() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        return std::move(this->items_);
    }

private:
    mutable std::mutex mutex_;
    std::vector<T> items_;
};

// Convenient alias
using StringQueue = ThreadSafeQueue<std::string>;

} // namespace termihui
