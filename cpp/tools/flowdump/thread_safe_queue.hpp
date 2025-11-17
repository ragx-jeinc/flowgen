#ifndef FLOWDUMP_THREAD_SAFE_QUEUE_HPP
#define FLOWDUMP_THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <optional>

namespace flowdump {

/**
 * Thread-safe queue with blocking operations and done flag
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() : done_(false) {}

    /**
     * Push item to queue
     */
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cond_.notify_one();
    }

    /**
     * Try to pop item from queue with timeout
     * Returns empty optional if timeout expires or queue is done
     */
    std::optional<T> try_pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (!cond_.wait_for(lock, timeout, [this] {
            return !queue_.empty() || done_;
        })) {
            // Timeout
            return std::nullopt;
        }

        if (queue_.empty()) {
            // Queue is done and empty
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * Pop item from queue (blocking)
     * Returns empty optional if queue is done
     */
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);

        cond_.wait(lock, [this] {
            return !queue_.empty() || done_;
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * Mark queue as done (no more items will be pushed)
     */
    void set_done() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
        }
        cond_.notify_all();
    }

    /**
     * Check if queue is done
     */
    bool is_done() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return done_;
    }

    /**
     * Get current queue size
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * Check if queue is empty
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    bool done_;
};

} // namespace flowdump

#endif // FLOWDUMP_THREAD_SAFE_QUEUE_HPP
