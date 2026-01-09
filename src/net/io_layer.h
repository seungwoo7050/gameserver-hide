#pragma once

#include "net/codec.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace net {

enum class IoPlatform {
    WindowsIocp,
    LinuxEpoll,
};

struct IoConfig {
    IoPlatform platform{IoPlatform::LinuxEpoll};
    std::size_t acceptor_threads{1};
    std::size_t event_loop_threads{1};
    std::size_t worker_threads{4};
};

IoPlatform defaultIoPlatform();

struct IoEvent {
    enum class Type {
        Accept,
        Read,
        Write,
        Disconnect,
    };

    Type type{Type::Read};
    std::uint64_t connection_id{0};
    std::vector<std::uint8_t> payload{};
    std::size_t bytes_transferred{0};
};

class PacketPipeline {
public:
    using DispatchFn = std::function<void(std::uint64_t,
                                          const FrameHeader &,
                                          const std::vector<std::uint8_t> &,
                                          std::chrono::steady_clock::time_point)>;

    explicit PacketPipeline(DispatchFn dispatch);

    void registerConnection(std::uint64_t connection_id);
    void removeConnection(std::uint64_t connection_id);
    void onRead(std::uint64_t connection_id,
                const std::vector<std::uint8_t> &payload,
                std::chrono::steady_clock::time_point now);

private:
    DispatchFn dispatch_;
    std::unordered_map<std::uint64_t, FrameDecoder> decoders_;
};

class IoEventLoop {
public:
    using AcceptHandler =
        std::function<void(std::uint64_t, std::chrono::steady_clock::time_point)>;
    using ReadHandler = std::function<void(std::uint64_t,
                                           const std::vector<std::uint8_t> &,
                                           std::chrono::steady_clock::time_point)>;
    using WriteHandler = std::function<void(std::uint64_t,
                                            std::size_t,
                                            std::chrono::steady_clock::time_point)>;
    using DisconnectHandler =
        std::function<void(std::uint64_t, std::chrono::steady_clock::time_point)>;

    void setAcceptHandler(AcceptHandler handler);
    void setReadHandler(ReadHandler handler);
    void setWriteHandler(WriteHandler handler);
    void setDisconnectHandler(DisconnectHandler handler);

    void enqueueEvent(IoEvent event);
    void drain(std::chrono::steady_clock::time_point now);

private:
    std::vector<IoEvent> pending_;
    AcceptHandler on_accept_;
    ReadHandler on_read_;
    WriteHandler on_write_;
    DisconnectHandler on_disconnect_;
};

}  // namespace net
