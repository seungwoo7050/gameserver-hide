#include "guild/guild.h"

#include <algorithm>

namespace guild {

std::optional<GuildId> GuildService::createGuild(SessionId leader_session_id,
                                                 std::string leader_user_id,
                                                 std::string guild_name) {
    if (member_index_.count(leader_session_id) > 0 || guild_name.empty()) {
        return std::nullopt;
    }

    GuildRecord record;
    record.id = next_guild_id_++;
    record.leader_session_id = leader_session_id;
    record.name = std::move(guild_name);
    record.members.emplace(leader_session_id,
                           GuildMember{leader_session_id, leader_user_id});
    guilds_.emplace(record.id, record);
    member_index_[leader_session_id] = record.id;

    GuildEvent event;
    event.type = GuildEventType::Created;
    event.guild_id = record.id;
    event.actor_user_id = std::move(leader_user_id);
    event.member_user_ids.push_back(event.actor_user_id);
    event.message = "Guild created";
    emitToGuild(record, event);

    return record.id;
}

bool GuildService::joinGuild(GuildId guild_id,
                             SessionId member_session_id,
                             std::string member_user_id) {
    auto guild_it = guilds_.find(guild_id);
    if (guild_it == guilds_.end()) {
        return false;
    }

    if (member_index_.count(member_session_id) > 0) {
        return false;
    }

    auto &guild = guild_it->second;
    guild.members.emplace(member_session_id,
                          GuildMember{member_session_id, member_user_id});
    member_index_[member_session_id] = guild_id;

    GuildEvent event;
    event.type = GuildEventType::Joined;
    event.guild_id = guild_id;
    event.actor_user_id = std::move(member_user_id);
    event.member_user_ids.reserve(guild.members.size());
    for (const auto &entry : guild.members) {
        event.member_user_ids.push_back(entry.second.user_id);
    }
    event.message = "Guild member joined";
    emitToGuild(guild, event);
    return true;
}

bool GuildService::leaveGuild(GuildId guild_id, SessionId member_session_id) {
    auto guild_it = guilds_.find(guild_id);
    if (guild_it == guilds_.end()) {
        return false;
    }

    auto &guild = guild_it->second;
    auto member_it = guild.members.find(member_session_id);
    if (member_it == guild.members.end()) {
        return false;
    }

    std::string actor_user_id = member_it->second.user_id;
    bool is_leader = guild.leader_session_id == member_session_id;
    if (is_leader) {
        GuildEvent event;
        event.type = GuildEventType::Disbanded;
        event.guild_id = guild_id;
        event.actor_user_id = actor_user_id;
        event.member_user_ids.reserve(guild.members.size());
        for (const auto &entry : guild.members) {
            event.member_user_ids.push_back(entry.second.user_id);
            member_index_.erase(entry.first);
        }
        event.message = "Guild disbanded";
        emitToGuild(guild, event);
        guilds_.erase(guild_it);
        return true;
    }

    guild.members.erase(member_it);
    member_index_.erase(member_session_id);

    GuildEvent event;
    event.type = GuildEventType::Left;
    event.guild_id = guild_id;
    event.actor_user_id = std::move(actor_user_id);
    event.member_user_ids.reserve(guild.members.size());
    for (const auto &entry : guild.members) {
        event.member_user_ids.push_back(entry.second.user_id);
    }
    event.message = "Guild member left";
    emitToGuild(guild, event);
    emitToMember(member_session_id, event);
    return true;
}

bool GuildService::removeMember(SessionId member_session_id) {
    auto member_it = member_index_.find(member_session_id);
    if (member_it == member_index_.end()) {
        return false;
    }
    GuildId guild_id = member_it->second;
    auto guild_it = guilds_.find(guild_id);
    if (guild_it == guilds_.end()) {
        member_index_.erase(member_it);
        return false;
    }

    auto &guild = guild_it->second;
    auto entry = guild.members.find(member_session_id);
    if (entry == guild.members.end()) {
        member_index_.erase(member_it);
        return false;
    }

    std::string actor_user_id = entry->second.user_id;
    if (guild.leader_session_id == member_session_id) {
        GuildEvent event;
        event.type = GuildEventType::Disbanded;
        event.guild_id = guild_id;
        event.actor_user_id = actor_user_id;
        event.member_user_ids.reserve(guild.members.size());
        for (const auto &member : guild.members) {
            event.member_user_ids.push_back(member.second.user_id);
            member_index_.erase(member.first);
        }
        event.message = "Guild disbanded";
        emitToGuild(guild, event);
        guilds_.erase(guild_it);
        return true;
    }

    guild.members.erase(entry);
    member_index_.erase(member_it);

    GuildEvent event;
    event.type = GuildEventType::Left;
    event.guild_id = guild_id;
    event.actor_user_id = std::move(actor_user_id);
    event.member_user_ids.reserve(guild.members.size());
    for (const auto &member : guild.members) {
        event.member_user_ids.push_back(member.second.user_id);
    }
    event.message = "Guild member left";
    emitToGuild(guild, event);
    return true;
}

std::optional<GuildInfo> GuildService::getGuildInfo(GuildId guild_id) const {
    auto it = guilds_.find(guild_id);
    if (it == guilds_.end()) {
        return std::nullopt;
    }

    GuildInfo info;
    info.guild_id = it->second.id;
    info.leader_session_id = it->second.leader_session_id;
    info.name = it->second.name;
    info.members.reserve(it->second.members.size());
    for (const auto &entry : it->second.members) {
        info.members.push_back(entry.second);
    }
    std::sort(info.members.begin(), info.members.end(),
              [](const GuildMember &lhs, const GuildMember &rhs) {
                  return lhs.session_id < rhs.session_id;
              });
    return info;
}

std::optional<GuildId> GuildService::guildForMember(SessionId session_id) const {
    auto it = member_index_.find(session_id);
    if (it == member_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void GuildService::setEventSink(EventSink sink) {
    event_sink_ = std::move(sink);
}

void GuildService::emitToGuild(const GuildRecord &guild, const GuildEvent &event) {
    if (!event_sink_) {
        return;
    }
    for (const auto &member : guild.members) {
        event_sink_(member.first, event);
    }
}

void GuildService::emitToMember(SessionId member_session_id,
                                const GuildEvent &event) {
    if (!event_sink_) {
        return;
    }
    event_sink_(member_session_id, event);
}

}  // namespace guild
