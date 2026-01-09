#include "net/io_layer.h"

namespace net {

IoPlatform defaultIoPlatform() {
#if defined(_WIN32)
    return IoPlatform::WindowsIocp;
#else
    return IoPlatform::LinuxEpoll;
#endif
}

PacketPipeline::PacketPipeline(DispatchFn dispatch)
    : dispatch_(std::move(dispatch)) {}

void PacketPipeline::registerConnection(std::uint64_t connection_id) {
    decoders_.emplace(connection_id, FrameDecoder{});
}

void PacketPipeline::removeConnection(std::uint64_t connection_id) {
    decoders_.erase(connection_id);
}

void PacketPipeline::onRead(std::uint64_t connection_id,
                            const std::vector<std::uint8_t> &payload,
                            std::chrono::steady_clock::time_point now) {
    auto it = decoders_.find(connection_id);
    if (it == decoders_.end()) {
        return;
    }
    it->second.append(payload);
    FrameHeader header{};
    std::vector<std::uint8_t> frame_payload;
    while (it->second.nextFrame(header, frame_payload)) {
        if (dispatch_) {
            dispatch_(connection_id, header, frame_payload, now);
        }
        frame_payload.clear();
    }
}

void IoEventLoop::setAcceptHandler(AcceptHandler handler) {
    on_accept_ = std::move(handler);
}

void IoEventLoop::setReadHandler(ReadHandler handler) {
    on_read_ = std::move(handler);
}

void IoEventLoop::setWriteHandler(WriteHandler handler) {
    on_write_ = std::move(handler);
}

void IoEventLoop::setDisconnectHandler(DisconnectHandler handler) {
    on_disconnect_ = std::move(handler);
}

void IoEventLoop::enqueueEvent(IoEvent event) {
    pending_.push_back(std::move(event));
}

void IoEventLoop::drain(std::chrono::steady_clock::time_point now) {
    for (const auto &event : pending_) {
        switch (event.type) {
            case IoEvent::Type::Accept:
                if (on_accept_) {
                    on_accept_(event.connection_id, now);
                }
                break;
            case IoEvent::Type::Read:
                if (on_read_) {
                    on_read_(event.connection_id, event.payload, now);
                }
                break;
            case IoEvent::Type::Write:
                if (on_write_) {
                    on_write_(event.connection_id, event.bytes_transferred, now);
                }
                break;
            case IoEvent::Type::Disconnect:
                if (on_disconnect_) {
                    on_disconnect_(event.connection_id, now);
                }
                break;
        }
    }
    pending_.clear();
}

}  // namespace net
