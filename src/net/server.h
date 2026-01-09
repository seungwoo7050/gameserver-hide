#pragma once

#include "admin/logging.h"
#include "chat/chat.h"
#include "guild/guild.h"
#include "inventory/inventory_storage.h"
#include "match/match_queue.h"
#include "net/auth.h"
#include "net/codec.h"
#include "net/protocol.h"
#include "net/security.h"
#include "net/session.h"
#include "party/party.h"
#include "dungeon/instance_manager.h"
#include "reward/reward_service.h"

#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>

namespace net {

struct SecurityPolicy {
    bool require_tls{false};
    bool require_hmac{false};
    bool enable_replay_protection{false};
    std::string hmac_key{"dev-secret"};
};

class Server {
public:
    using SessionId = Session::SessionId;

    struct Metrics {
        std::uint64_t packets_total{0};
        std::uint64_t bytes_total{0};
        std::uint64_t error_total{0};
    };

    explicit Server(std::shared_ptr<inventory::InventoryStorage> inventory_storage = nullptr,
                    SecurityPolicy security_policy = SecurityPolicy{});

    std::shared_ptr<Session> createSession(
        const SessionConfig &config,
        std::chrono::steady_clock::time_point now);
    void removeSession(SessionId id);
    std::shared_ptr<Session> findSession(SessionId id) const;

    void tick(std::chrono::steady_clock::time_point now);
    std::size_t sessionCount() const;
    Metrics metrics() const;
    std::chrono::steady_clock::time_point startTime() const;

    std::optional<std::vector<std::uint8_t>> handlePacket(
        Session &session,
        const FrameHeader &header,
        const std::vector<std::uint8_t> &payload,
        std::chrono::steady_clock::time_point now);

    const Session::UserContext *sessionUser(SessionId id) const;
    party::PartyService &partyService();
    dungeon::InstanceManager &instanceManager();
    bool forceDisconnect(SessionId id,
                         const std::string &reason,
                         const std::string &request_trace_id);

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
    match::MatchQueue match_queue_{match::MatchRule{}};
    dungeon::InstanceManager instance_manager_;
    std::shared_ptr<inventory::InventoryStorage> inventory_storage_;
    reward::RewardService reward_service_;
    std::unordered_map<party::PartyId, dungeon::InstanceId> party_instances_;
    std::unordered_map<dungeon::InstanceId, std::string> instance_tickets_;
    std::unordered_map<dungeon::InstanceId, std::uint32_t> instance_seeds_;
    std::unordered_map<dungeon::InstanceId, reward::GrantId> instance_reward_grants_;
    std::unordered_map<SessionId, dungeon::InstanceId> session_instances_;
    std::unordered_map<SessionId, std::uint64_t> session_characters_;
    std::mt19937 rng_{std::random_device{}()};
    std::uint64_t next_reward_grant_id_{1};
    Metrics metrics_{};
    std::chrono::steady_clock::time_point started_at_;
    admin::StructuredLogger logger_{};
    SecurityPolicy security_policy_{};
};

}  // namespace net
