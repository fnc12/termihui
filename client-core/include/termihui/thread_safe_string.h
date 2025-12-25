#pragma once

#include <mutex>
#include <string>
#include <optional>

namespace termihui {

/**
 * Thread-safe string wrapper
 * For single values that need atomic get-and-clear
 */
class ThreadSafeString {
public:
    /**
     * Set value (overwrites previous)
     */
    void set(std::string value) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->value_ = std::move(value);
    }
    
    /**
     * Take value and clear atomically
     * @return value or nullopt if empty
     */
    std::optional<std::string> take() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        if (this->value_.empty()) {
            return std::nullopt;
        }
        std::string result = std::move(this->value_);
        this->value_.clear();
        return result;
    }
    
    /**
     * Clear value
     */
    void clear() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        this->value_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::string value_;
};

} // namespace termihui

