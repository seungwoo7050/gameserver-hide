#include "net/worker_pool.h"

namespace net {

WorkerPool::WorkerPool(std::size_t thread_count)
    : thread_count_(thread_count) {}

WorkerPool::~WorkerPool() {
    stop();
}

void WorkerPool::start() {
    if (running_) {
        return;
    }
    running_ = true;
    stop_requested_ = false;
    threads_.reserve(thread_count_);
    for (std::size_t i = 0; i < thread_count_; ++i) {
        threads_.emplace_back(&WorkerPool::workerLoop, this);
    }
}

void WorkerPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_requested_ = true;
    }
    cv_.notify_all();
    for (auto &thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    running_ = false;
}

void WorkerPool::submit(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }
    cv_.notify_one();
}

void WorkerPool::workerLoop() {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_requested_ || !jobs_.empty(); });
            if (stop_requested_ && jobs_.empty()) {
                return;
            }
            job = std::move(jobs_.front());
            jobs_.pop();
        }
        if (job) {
            job();
        }
    }
}

PacketQueue::PacketQueue(Config config) : config_(config) {}

bool PacketQueue::push(PacketJob job) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (config_.capacity > 0) {
        switch (config_.overflow_policy) {
            case Config::OverflowPolicy::Block:
                cv_.wait(lock, [this] { return stopped_ || !isFullLocked(); });
                if (stopped_) {
                    return false;
                }
                break;
            case Config::OverflowPolicy::DropNewest:
                if (isFullLocked()) {
                    dropped_ += 1;
                    return false;
                }
                break;
            case Config::OverflowPolicy::DropOldest:
                if (isFullLocked()) {
                    queue_.pop();
                    dropped_ += 1;
                }
                break;
        }
    }
    queue_.push(std::move(job));
    lock.unlock();
    cv_.notify_one();
    return true;
}

bool PacketQueue::pop(PacketJob &job) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    if (stopped_ && queue_.empty()) {
        return false;
    }
    job = std::move(queue_.front());
    queue_.pop();
    if (config_.capacity > 0) {
        lock.unlock();
        cv_.notify_one();
    }
    return true;
}

void PacketQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
}

std::size_t PacketQueue::droppedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_;
}

bool PacketQueue::isFullLocked() const {
    if (config_.capacity == 0) {
        return false;
    }
    return queue_.size() >= config_.capacity;
}

PacketDispatcher::PacketDispatcher(std::size_t worker_threads,
                                   JobHandler handler,
                                   PacketQueue::Config queue_config)
    : worker_pool_(worker_threads),
      queue_(queue_config),
      handler_(std::move(handler)) {}

PacketDispatcher::~PacketDispatcher() {
    stop();
}

void PacketDispatcher::start() {
    if (running_) {
        return;
    }
    running_ = true;
    worker_pool_.start();
    dispatcher_ = std::thread(&PacketDispatcher::dispatchLoop, this);
}

void PacketDispatcher::stop() {
    if (!running_) {
        return;
    }
    queue_.stop();
    if (dispatcher_.joinable()) {
        dispatcher_.join();
    }
    worker_pool_.stop();
    running_ = false;
}

void PacketDispatcher::enqueue(PacketJob job) {
    queue_.push(std::move(job));
}

void PacketDispatcher::dispatchLoop() {
    PacketJob job;
    while (queue_.pop(job)) {
        if (!handler_) {
            continue;
        }
        worker_pool_.submit([handler = handler_, job]() { handler(job); });
    }
}

}  // namespace net
