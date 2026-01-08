#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace guild {

using GuildId = std::uint64_t;
using SessionId = std::uint64_t;

enum class GuildEventType : std::uint16_t {
    Created = 1,
    Joined = 2,
    Left = 3,
    Disbanded = 4
};

struct GuildEvent {
    GuildEventType type{GuildEventType::Created};
    GuildId guild_id{0};
    std::string actor_user_id;
    std::vector<std::string> member_user_ids;
    std::string message;
};

struct GuildMember {
    SessionId session_id{0};
    std::string user_id;
};

struct GuildInfo {
    GuildId guild_id{0};
    SessionId leader_session_id{0};
    std::string name;
    std::vector<GuildMember> members;
};

class GuildService {
public:
    using EventSink = std::function<void(SessionId, const GuildEvent &event)>;

    std::optional<GuildId> createGuild(SessionId leader_session_id,
                                       std::string leader_user_id,
                                       std::string guild_name);
    bool joinGuild(GuildId guild_id,
                   SessionId member_session_id,
                   std::string member_user_id);
    bool leaveGuild(GuildId guild_id, SessionId member_session_id);
    bool removeMember(SessionId member_session_id);
    bool replaceMemberSession(SessionId old_session_id, SessionId new_session_id);

    std::optional<GuildInfo> getGuildInfo(GuildId guild_id) const;
    std::optional<GuildId> guildForMember(SessionId session_id) const;

    void setEventSink(EventSink sink);

private:
    struct GuildRecord {
        GuildId id{0};
        SessionId leader_session_id{0};
        std::string name;
        std::unordered_map<SessionId, GuildMember> members;
    };

    void emitToGuild(const GuildRecord &guild, const GuildEvent &event);
    void emitToMember(SessionId member_session_id, const GuildEvent &event);

    GuildId next_guild_id_{1};
    std::unordered_map<GuildId, GuildRecord> guilds_;
    std::unordered_map<SessionId, GuildId> member_index_;
    EventSink event_sink_;
};

}  // namespace guild
