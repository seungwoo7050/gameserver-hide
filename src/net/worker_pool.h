#pragma once

#include "net/codec.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace net {

class WorkerPool {
public:
    explicit WorkerPool(std::size_t thread_count);
    ~WorkerPool();

    WorkerPool(const WorkerPool &) = delete;
    WorkerPool &operator=(const WorkerPool &) = delete;

    void start();
    void stop();
    void submit(std::function<void()> job);

private:
    void workerLoop();

    std::size_t thread_count_{0};
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_{false};
    bool stop_requested_{false};
};

struct PacketJob {
    std::uint64_t connection_id{0};
    FrameHeader header{};
    std::vector<std::uint8_t> payload{};
    std::chrono::steady_clock::time_point received_at{};
};

class PacketQueue {
public:
    struct Config {
        enum class OverflowPolicy {
            Block,
            DropNewest,
            DropOldest,
        };

        std::size_t capacity;
        OverflowPolicy overflow_policy;

        Config(std::size_t capacity = 0,
               OverflowPolicy overflow_policy = OverflowPolicy::DropNewest)
            : capacity(capacity), overflow_policy(overflow_policy) {}
    };

    explicit PacketQueue(Config config = Config());

    bool push(PacketJob job);
    bool pop(PacketJob &job);
    void stop();
    std::size_t droppedCount() const;

private:
    bool isFullLocked() const;

    std::queue<PacketJob> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_{false};
    Config config_{};
    std::size_t dropped_{0};
};

class PacketDispatcher {
public:
    using JobHandler = std::function<void(const PacketJob &)>;

    PacketDispatcher(std::size_t worker_threads,
                     JobHandler handler,
                     PacketQueue::Config queue_config = PacketQueue::Config());
    ~PacketDispatcher();

    PacketDispatcher(const PacketDispatcher &) = delete;
    PacketDispatcher &operator=(const PacketDispatcher &) = delete;

    void start();
    void stop();
    void enqueue(PacketJob job);

private:
    void dispatchLoop();

    WorkerPool worker_pool_;
    PacketQueue queue_;
    JobHandler handler_;
    std::thread dispatcher_;
    bool running_{false};
};

}  // namespace net
