#pragma once

#include "chat/chat.h"
#include "guild/guild.h"
#include "net/auth.h"
#include "net/codec.h"
#include "net/protocol.h"
#include "net/session.h"
#include "party/party.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace net {

class Server {
public:
    using SessionId = Session::SessionId;

    Server();

    std::shared_ptr<Session> createSession(
        const SessionConfig &config,
        std::chrono::steady_clock::time_point now);
    void removeSession(SessionId id);
    std::shared_ptr<Session> findSession(SessionId id) const;

    void tick(std::chrono::steady_clock::time_point now);
    std::size_t sessionCount() const;

    std::optional<std::vector<std::uint8_t>> handlePacket(
        Session &session,
        const FrameHeader &header,
        const std::vector<std::uint8_t> &payload,
        std::chrono::steady_clock::time_point now);

    const Session::UserContext *sessionUser(SessionId id) const;
    party::PartyService &partyService();

private:
    using SessionRecord = Session::UserContext;

    class SessionRegistry {
    public:
        bool registerSession(SessionId id, SessionRecord record);
        void removeSession(SessionId id);
        const SessionRecord *find(SessionId id) const;
        bool hasUser(const std::string &user_id, SessionId &session_id) const;

    private:
        std::unordered_map<SessionId, SessionRecord> records_;
        std::unordered_map<std::string, SessionId> active_users_;
    };

    SessionId next_id_{1};
    std::unordered_map<SessionId, std::shared_ptr<Session>> sessions_;
    SessionRegistry registry_;
    TokenService token_service_;
    party::PartyService party_service_;
    guild::GuildService guild_service_;
    chat::ChatService chat_service_;
};

}  // namespace net
