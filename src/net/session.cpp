#include "net/session.h"

#include "admin/logging.h"

#include <algorithm>

namespace net {
namespace {

admin::StructuredLogger &sessionLogger() {
    static admin::StructuredLogger logger;
    return logger;
}

}  // namespace

bool TokenBucket::consume(double amount, std::chrono::steady_clock::time_point now) {
    auto elapsed = std::chrono::duration<double>(now - last_refill).count();
    tokens = std::min(capacity, tokens + elapsed * refill_rate);
    last_refill = now;
    if (tokens >= amount) {
        tokens -= amount;
        return true;
    }
    return false;
}

Session::Session(SessionId id, const SessionConfig &config,
                 std::chrono::steady_clock::time_point now)
    : id_(id),
      config_(config),
      bucket_{config.rate_limit_capacity,
              config.rate_limit_capacity,
              config.rate_limit_refill_per_sec,
              now},
      last_activity_(now),
      last_receive_(now),
      last_heartbeat_(now),
      trace_id_(admin::StructuredLogger::generateTraceId()) {}

Session::SessionId Session::id() const {
    return id_;
}

bool Session::connected() const {
    return connected_;
}

void Session::onReceive(std::chrono::steady_clock::time_point now) {
    last_receive_ = now;
    last_activity_ = now;
}

bool Session::enqueueSend(std::vector<std::uint8_t> payload,
                          std::chrono::steady_clock::time_point now) {
    if (!connected_) {
        return false;
    }

    if (!bucket_.consume(static_cast<double>(payload.size()), now)) {
        admin::LogFields fields;
        fields.session_id = id_;
        fields.session_trace_id = trace_id_;
        fields.bytes = payload.size();
        sessionLogger().log("warn", "session_rate_limited",
                            "Session rate limited", fields);
        return false;
    }

    std::size_t next_size = send_queue_bytes_ + payload.size();
    if (next_size > config_.send_queue_limit_bytes) {
        if (config_.overflow_policy == OverflowPolicy::Disconnect) {
            disconnect("send queue overflow");
            return false;
        }
        if (config_.overflow_policy == OverflowPolicy::DropOldest) {
            while (!send_queue_.empty() && next_size > config_.send_queue_limit_bytes) {
                next_size -= send_queue_.front().size();
                send_queue_bytes_ -= send_queue_.front().size();
                send_queue_.pop_front();
            }
        } else {
            admin::LogFields fields;
            fields.session_id = id_;
            fields.session_trace_id = trace_id_;
            fields.bytes = payload.size();
            sessionLogger().log("warn", "session_queue_overflow",
                                "Session send queue overflow", fields);
            return false;
        }
    }

    send_queue_bytes_ = next_size;
    send_queue_.push_back(std::move(payload));
    last_activity_ = now;
    return true;
}

bool Session::shouldSendHeartbeat(std::chrono::steady_clock::time_point now) const {
    return connected_ && (now - last_heartbeat_) >= config_.heartbeat_interval;
}

void Session::markHeartbeatSent(std::chrono::steady_clock::time_point now) {
    last_heartbeat_ = now;
}

bool Session::tick(std::chrono::steady_clock::time_point now) {
    if (!connected_) {
        return false;
    }

    if ((now - last_receive_) >= config_.timeout) {
        disconnect("timeout");
    }

    return connected_;
}

std::size_t Session::queuedBytes() const {
    return send_queue_bytes_;
}

void Session::attachUserContext(UserContext context) {
    user_context_ = std::move(context);
}

void Session::clearUserContext() {
    user_context_.reset();
}

const std::optional<Session::UserContext> &Session::userContext() const {
    return user_context_;
}

const std::string &Session::traceId() const {
    return trace_id_;
}

void Session::setProtocolVersion(std::uint16_t version) {
    protocol_version_ = version;
}

std::uint16_t Session::protocolVersion() const {
    return protocol_version_;
}

bool Session::dequeueSend(std::vector<std::uint8_t> &payload) {
    if (send_queue_.empty()) {
        return false;
    }
    payload = std::move(send_queue_.front());
    send_queue_bytes_ -= payload.size();
    send_queue_.pop_front();
    return true;
}

void Session::disconnect(const char *reason) {
    if (!connected_) {
        return;
    }
    connected_ = false;
    admin::LogFields fields;
    fields.session_id = id_;
    fields.session_trace_id = trace_id_;
    fields.reason = reason;
    sessionLogger().log("info", "session_disconnected", "Session disconnected",
                        fields);
}

}  // namespace net
