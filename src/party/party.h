#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace party {

using PartyId = std::uint64_t;
using SessionId = std::uint64_t;

enum class PartyEventType : std::uint16_t {
    Created = 1,
    InviteSent = 2,
    InviteAccepted = 3,
    InviteRejected = 4,
    InviteExpired = 5,
    Disbanded = 6
};

struct PartyEvent {
    PartyEventType type{PartyEventType::Created};
    PartyId party_id{0};
    SessionId actor_session_id{0};
    SessionId target_session_id{0};
    std::vector<SessionId> member_session_ids;
    std::string message;
};

struct PartyMember {
    SessionId session_id{0};
    std::string user_id;
};

struct PartyInfo {
    PartyId party_id{0};
    SessionId leader_session_id{0};
    std::vector<PartyMember> members;
};

class PartyService {
public:
    using EventSink = std::function<void(SessionId, const PartyEvent &event)>;

    PartyService();

    std::optional<PartyId> createParty(SessionId leader_session_id,
                                       std::string leader_user_id);
    bool inviteMember(PartyId party_id,
                      SessionId inviter_session_id,
                      SessionId invitee_session_id,
                      std::string invitee_user_id,
                      std::chrono::steady_clock::time_point now);
    bool acceptInvite(PartyId party_id,
                      SessionId invitee_session_id,
                      std::chrono::steady_clock::time_point now);
    bool rejectInvite(PartyId party_id, SessionId invitee_session_id);
    bool disbandParty(PartyId party_id, SessionId requester_session_id);
    bool removeMember(SessionId member_session_id);
    bool replaceMemberSession(SessionId old_session_id, SessionId new_session_id);
    std::size_t expireInvites(std::chrono::steady_clock::time_point now);

    std::optional<PartyInfo> getPartyInfo(PartyId party_id) const;
    std::optional<PartyId> partyForMember(SessionId session_id) const;

    void setEventSink(EventSink sink);
    void setInviteTimeout(std::chrono::milliseconds timeout);

private:
    struct PartyRecord {
        PartyId id{0};
        SessionId leader_session_id{0};
        std::unordered_map<SessionId, PartyMember> members;
    };

    struct PartyInvite {
        SessionId inviter_session_id{0};
        std::string invitee_user_id;
        std::chrono::steady_clock::time_point sent_at{};
    };

    void emitToParty(const PartyRecord &party, const PartyEvent &event);
    void emitToInvitee(SessionId invitee_session_id, const PartyEvent &event);

    PartyId next_party_id_{1};
    std::unordered_map<PartyId, PartyRecord> parties_;
    std::unordered_map<SessionId, PartyId> member_index_;
    std::unordered_map<PartyId, std::unordered_map<SessionId, PartyInvite>> invites_;
    EventSink event_sink_;
    std::chrono::milliseconds invite_timeout_{std::chrono::minutes{5}};
};

}  // namespace party
