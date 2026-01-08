#include "party/party.h"

#include <algorithm>

namespace party {

PartyService::PartyService() = default;

std::optional<PartyId> PartyService::createParty(SessionId leader_session_id,
                                                 std::string leader_user_id) {
    if (member_index_.count(leader_session_id) > 0) {
        return std::nullopt;
    }

    PartyRecord record;
    record.id = next_party_id_++;
    record.leader_session_id = leader_session_id;
    record.members.emplace(leader_session_id,
                           PartyMember{leader_session_id, std::move(leader_user_id)});

    member_index_[leader_session_id] = record.id;
    parties_.emplace(record.id, record);

    PartyEvent event;
    event.type = PartyEventType::Created;
    event.party_id = record.id;
    event.actor_session_id = leader_session_id;
    event.member_session_ids.push_back(leader_session_id);
    event.message = "Party created";
    emitToParty(record, event);

    return record.id;
}

bool PartyService::inviteMember(PartyId party_id,
                                SessionId inviter_session_id,
                                SessionId invitee_session_id,
                                std::string invitee_user_id,
                                std::chrono::steady_clock::time_point now) {
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        return false;
    }

    auto &party = party_it->second;
    if (party.members.count(inviter_session_id) == 0) {
        return false;
    }

    if (member_index_.count(invitee_session_id) > 0) {
        return false;
    }

    auto &party_invites = invites_[party_id];
    if (party_invites.count(invitee_session_id) > 0) {
        return false;
    }

    party_invites.emplace(
        invitee_session_id,
        PartyInvite{inviter_session_id, std::move(invitee_user_id), now});

    PartyEvent event;
    event.type = PartyEventType::InviteSent;
    event.party_id = party_id;
    event.actor_session_id = inviter_session_id;
    event.target_session_id = invitee_session_id;
    event.message = "Party invite sent";
    emitToParty(party, event);
    emitToInvitee(invitee_session_id, event);
    return true;
}

bool PartyService::acceptInvite(PartyId party_id,
                                SessionId invitee_session_id,
                                std::chrono::steady_clock::time_point now) {
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        return false;
    }
    auto invite_it = invites_.find(party_id);
    if (invite_it == invites_.end()) {
        return false;
    }
    auto &party_invites = invite_it->second;
    auto entry = party_invites.find(invitee_session_id);
    if (entry == party_invites.end()) {
        return false;
    }
    if (now - entry->second.sent_at > invite_timeout_) {
        PartyEvent expired;
        expired.type = PartyEventType::InviteExpired;
        expired.party_id = party_id;
        expired.target_session_id = invitee_session_id;
        expired.message = "Party invite expired";
        emitToParty(party_it->second, expired);
        emitToInvitee(invitee_session_id, expired);
        party_invites.erase(entry);
        return false;
    }
    if (member_index_.count(invitee_session_id) > 0) {
        party_invites.erase(entry);
        return false;
    }

    auto &party = party_it->second;
    party.members.emplace(invitee_session_id,
                          PartyMember{invitee_session_id, entry->second.invitee_user_id});
    member_index_[invitee_session_id] = party_id;
    party_invites.erase(entry);

    PartyEvent event;
    event.type = PartyEventType::InviteAccepted;
    event.party_id = party_id;
    event.actor_session_id = invitee_session_id;
    event.message = "Party invite accepted";
    event.member_session_ids.reserve(party.members.size());
    for (const auto &member_entry : party.members) {
        event.member_session_ids.push_back(member_entry.first);
    }
    emitToParty(party, event);
    return true;
}

bool PartyService::rejectInvite(PartyId party_id, SessionId invitee_session_id) {
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        return false;
    }
    auto invite_it = invites_.find(party_id);
    if (invite_it == invites_.end()) {
        return false;
    }
    auto &party_invites = invite_it->second;
    auto entry = party_invites.find(invitee_session_id);
    if (entry == party_invites.end()) {
        return false;
    }

    PartyEvent event;
    event.type = PartyEventType::InviteRejected;
    event.party_id = party_id;
    event.actor_session_id = invitee_session_id;
    event.target_session_id = invitee_session_id;
    event.message = "Party invite rejected";
    emitToParty(party_it->second, event);
    emitToInvitee(invitee_session_id, event);
    party_invites.erase(entry);
    return true;
}

bool PartyService::disbandParty(PartyId party_id, SessionId requester_session_id) {
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        return false;
    }

    auto &party = party_it->second;
    if (party.leader_session_id != requester_session_id) {
        return false;
    }

    PartyEvent event;
    event.type = PartyEventType::Disbanded;
    event.party_id = party_id;
    event.actor_session_id = requester_session_id;
    event.message = "Party disbanded";
    event.member_session_ids.reserve(party.members.size());
    for (const auto &member : party.members) {
        event.member_session_ids.push_back(member.first);
        member_index_.erase(member.first);
    }

    emitToParty(party, event);
    parties_.erase(party_it);
    invites_.erase(party_id);
    return true;
}

bool PartyService::removeMember(SessionId member_session_id) {
    auto member_it = member_index_.find(member_session_id);
    if (member_it == member_index_.end()) {
        return false;
    }

    PartyId party_id = member_it->second;
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        member_index_.erase(member_it);
        return false;
    }

    auto &party = party_it->second;
    if (party.leader_session_id == member_session_id) {
        PartyEvent event;
        event.type = PartyEventType::Disbanded;
        event.party_id = party_id;
        event.actor_session_id = member_session_id;
        event.message = "Party disbanded";
        event.member_session_ids.reserve(party.members.size());
        for (const auto &member : party.members) {
            event.member_session_ids.push_back(member.first);
            member_index_.erase(member.first);
        }
        emitToParty(party, event);
        parties_.erase(party_it);
        invites_.erase(party_id);
        return true;
    }

    party.members.erase(member_session_id);
    member_index_.erase(member_it);
    auto invite_it = invites_.find(party_id);
    if (invite_it != invites_.end()) {
        invite_it->second.erase(member_session_id);
    }
    return true;
}

std::size_t PartyService::expireInvites(std::chrono::steady_clock::time_point now) {
    std::size_t expired_count = 0;
    for (auto &party_entry : parties_) {
        auto invite_it = invites_.find(party_entry.first);
        if (invite_it == invites_.end()) {
            continue;
        }
        auto &party_invites = invite_it->second;
        for (auto it = party_invites.begin(); it != party_invites.end();) {
            if (now - it->second.sent_at > invite_timeout_) {
                PartyEvent event;
                event.type = PartyEventType::InviteExpired;
                event.party_id = party_entry.first;
                event.target_session_id = it->first;
                event.message = "Party invite expired";
                emitToParty(party_entry.second, event);
                emitToInvitee(it->first, event);
                it = party_invites.erase(it);
                ++expired_count;
            } else {
                ++it;
            }
        }
    }
    return expired_count;
}

std::optional<PartyInfo> PartyService::getPartyInfo(PartyId party_id) const {
    auto party_it = parties_.find(party_id);
    if (party_it == parties_.end()) {
        return std::nullopt;
    }

    PartyInfo info;
    info.party_id = party_it->second.id;
    info.leader_session_id = party_it->second.leader_session_id;
    info.members.reserve(party_it->second.members.size());
    for (const auto &entry : party_it->second.members) {
        info.members.push_back(entry.second);
    }
    std::sort(info.members.begin(), info.members.end(),
              [](const PartyMember &lhs, const PartyMember &rhs) {
                  return lhs.session_id < rhs.session_id;
              });
    return info;
}

std::optional<PartyId> PartyService::partyForMember(SessionId session_id) const {
    auto it = member_index_.find(session_id);
    if (it == member_index_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void PartyService::setEventSink(EventSink sink) {
    event_sink_ = std::move(sink);
}

void PartyService::setInviteTimeout(std::chrono::milliseconds timeout) {
    invite_timeout_ = timeout;
}

void PartyService::emitToParty(const PartyRecord &party, const PartyEvent &event) {
    if (!event_sink_) {
        return;
    }
    for (const auto &member : party.members) {
        event_sink_(member.first, event);
    }
}

void PartyService::emitToInvitee(SessionId invitee_session_id,
                                 const PartyEvent &event) {
    if (!event_sink_) {
        return;
    }
    event_sink_(invitee_session_id, event);
}

}  // namespace party
