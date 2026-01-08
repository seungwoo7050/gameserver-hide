#pragma once

#include "net/session.h"

#include <chrono>
#include <memory>
#include <unordered_map>

namespace net {

class Server {
public:
    using SessionId = Session::SessionId;

    std::shared_ptr<Session> createSession(
        const SessionConfig &config,
        std::chrono::steady_clock::time_point now);
    void removeSession(SessionId id);
    std::shared_ptr<Session> findSession(SessionId id) const;

    void tick(std::chrono::steady_clock::time_point now);
    std::size_t sessionCount() const;

private:
    SessionId next_id_{1};
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
};

}  // namespace net
