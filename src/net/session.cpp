#include "net/session.h"

#include <algorithm>
#include <iostream>

namespace net {

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
      last_heartbeat_(now) {}

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
        std::cerr << "Session " << id_ << " rate limited: " << payload.size()
                  << " bytes" << std::endl;
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
            std::cerr << "Session " << id_ << " send queue overflow, dropping packet"
                      << std::endl;
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

void Session::disconnect(const char *reason) {
    if (!connected_) {
        return;
    }
    connected_ = false;
    std::cerr << "Session " << id_ << " disconnected: " << reason << std::endl;
}

}  // namespace net
