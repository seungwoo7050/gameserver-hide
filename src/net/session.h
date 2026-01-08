#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace net {

enum class OverflowPolicy {
    DropNewest,
    DropOldest,
    Disconnect
};

struct SessionConfig {
    std::chrono::milliseconds heartbeat_interval{15000};
    std::chrono::milliseconds timeout{45000};
    std::size_t send_queue_limit_bytes{262144};
    OverflowPolicy overflow_policy{OverflowPolicy::DropNewest};
    double rate_limit_capacity{65536.0};
    double rate_limit_refill_per_sec{32768.0};
};

struct TokenBucket {
    double capacity;
    double tokens;
    double refill_rate;
    std::chrono::steady_clock::time_point last_refill;

    bool consume(double amount, std::chrono::steady_clock::time_point now);
};

class Session {
public:
    using SessionId = std::uint64_t;

    struct UserContext {
        std::string user_id;
        std::string token;
    };

    Session(SessionId id, const SessionConfig &config,
            std::chrono::steady_clock::time_point now);

    SessionId id() const;
    bool connected() const;

    void onReceive(std::chrono::steady_clock::time_point now);
    bool enqueueSend(std::vector<std::uint8_t> payload,
                     std::chrono::steady_clock::time_point now);

    bool shouldSendHeartbeat(std::chrono::steady_clock::time_point now) const;
    void markHeartbeatSent(std::chrono::steady_clock::time_point now);

    bool tick(std::chrono::steady_clock::time_point now);
    std::size_t queuedBytes() const;

    void attachUserContext(UserContext context);
    void clearUserContext();
    const std::optional<UserContext> &userContext() const;
    void setProtocolVersion(std::uint16_t version);
    std::uint16_t protocolVersion() const;
    bool dequeueSend(std::vector<std::uint8_t> &payload);

private:
    void disconnect(const char *reason);

    SessionId id_;
    SessionConfig config_;
    TokenBucket bucket_;
    bool connected_{true};
    std::chrono::steady_clock::time_point last_activity_;
    std::chrono::steady_clock::time_point last_receive_;
    std::chrono::steady_clock::time_point last_heartbeat_;
    std::deque<std::vector<std::uint8_t>> send_queue_;
    std::size_t send_queue_bytes_{0};
    std::optional<UserContext> user_context_;
    std::uint16_t protocol_version_{0};
};

}  // namespace net
