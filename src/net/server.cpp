#include "net/server.h"

#include <vector>

namespace net {

std::shared_ptr<Session> Server::createSession(
    const SessionConfig &config,
    std::chrono::steady_clock::time_point now) {
    auto session = std::make_shared<Session>(next_id_++, config, now);
    sessions_.emplace(session->id(), session);
    return session;
}

void Server::removeSession(SessionId id) {
    sessions_.erase(id);
}

std::shared_ptr<Session> Server::findSession(SessionId id) const {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return it->second;
}

void Server::tick(std::chrono::steady_clock::time_point now) {
    std::vector<SessionId> to_remove;
    for (const auto &entry : sessions_) {
        if (!entry.second->tick(now)) {
            to_remove.push_back(entry.first);
        }
    }
    for (SessionId id : to_remove) {
        removeSession(id);
    }
}

std::size_t Server::sessionCount() const {
    return sessions_.size();
}

}  // namespace net
