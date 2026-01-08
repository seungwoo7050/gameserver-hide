#include "net/server.h"

#include <optional>
#include <sstream>
#include <vector>

namespace net {

bool Server::SessionRegistry::registerSession(SessionId id, SessionRecord record) {
    auto existing = active_users_.find(record.user_id);
    if (existing != active_users_.end() && existing->second != id) {
        return false;
    }
    auto previous = records_.find(id);
    if (previous != records_.end()) {
        active_users_.erase(previous->second.user_id);
    }
    active_users_[record.user_id] = id;
    records_[id] = std::move(record);
    return true;
}

void Server::SessionRegistry::removeSession(SessionId id) {
    auto it = records_.find(id);
    if (it != records_.end()) {
        active_users_.erase(it->second.user_id);
        records_.erase(it);
    }
}

const Server::SessionRecord *Server::SessionRegistry::find(SessionId id) const {
    auto it = records_.find(id);
    if (it == records_.end()) {
        return nullptr;
    }
    return &it->second;
}

bool Server::SessionRegistry::hasUser(const std::string &user_id,
                                      SessionId &session_id) const {
    auto it = active_users_.find(user_id);
    if (it == active_users_.end()) {
        return false;
    }
    session_id = it->second;
    return true;
}

std::shared_ptr<Session> Server::createSession(
    const SessionConfig &config,
    std::chrono::steady_clock::time_point now) {
    auto session = std::make_shared<Session>(next_id_++, config, now);
    sessions_.emplace(session->id(), session);
    return session;
}

void Server::removeSession(SessionId id) {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        it->second->clearUserContext();
        sessions_.erase(it);
    }
    registry_.removeSession(id);
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

std::optional<std::vector<std::uint8_t>> Server::handlePacket(
    Session &session,
    const FrameHeader &header,
    const std::vector<std::uint8_t> &payload,
    std::chrono::steady_clock::time_point now) {
    session.onReceive(now);

    if (header.version < kMinProtocolVersion || header.version > kMaxProtocolVersion) {
        VersionReject reject;
        reject.min_version = kMinProtocolVersion;
        reject.max_version = kMaxProtocolVersion;
        reject.client_version = header.version;
        std::ostringstream message;
        message << "Unsupported client version " << header.version
                << " (supported " << kMinProtocolVersion << "-"
                << kMaxProtocolVersion << ")";
        reject.message = message.str();
        auto encoded = encodeVersionReject(reject);
        return Codec::encode(static_cast<std::uint16_t>(PacketType::VersionReject),
                             header.version,
                             encoded);
    }

    switch (static_cast<PacketType>(header.type)) {
        case PacketType::LoginReq: {
            LoginRequest request;
            if (!decodeLoginRequest(payload, request)) {
                LoginResponse response;
                response.accepted = false;
                response.message = "Malformed login payload";
                auto encoded = encodeLoginResponse(response);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            if (request.password != "letmein") {
                LoginResponse response;
                response.accepted = false;
                response.message = "Invalid credentials";
                auto encoded = encodeLoginResponse(response);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            SessionId existing_id = 0;
            if (registry_.hasUser(request.user_id, existing_id) &&
                existing_id != session.id()) {
                LoginResponse response;
                response.accepted = false;
                response.message = "User already logged in";
                auto encoded = encodeLoginResponse(response);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            auto token = token_service_.issueToken(request.user_id, now);
            Session::UserContext context{request.user_id, token};
            session.attachUserContext(context);
            if (!registry_.registerSession(session.id(), {request.user_id, token})) {
                LoginResponse response;
                response.accepted = false;
                response.message = "User already logged in";
                auto encoded = encodeLoginResponse(response);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            LoginResponse response;
            response.accepted = true;
            response.token = token;
            response.message = "Login accepted";
            auto encoded = encodeLoginResponse(response);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::LogoutReq: {
            LogoutRequest request;
            if (!decodeLogoutRequest(payload, request)) {
                LogoutResponse response;
                response.success = false;
                response.message = "Malformed logout payload";
                auto encoded = encodeLogoutResponse(response);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LogoutRes),
                                     header.version,
                                     encoded);
            }

            session.clearUserContext();
            registry_.removeSession(session.id());

            LogoutResponse response;
            response.success = true;
            response.message = "Logout successful";
            auto encoded = encodeLogoutResponse(response);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::LogoutRes),
                                 header.version,
                                 encoded);
        }
        default:
            break;
    }

    return std::nullopt;
}

const Session::UserContext *Server::sessionUser(SessionId id) const {
    return registry_.find(id);
}

}  // namespace net
