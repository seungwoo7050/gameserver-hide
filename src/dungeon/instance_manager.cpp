#include "dungeon/instance_manager.h"

#include <algorithm>

namespace dungeon {

InstanceManager::InstanceManager() = default;

std::optional<InstanceId> InstanceManager::createInstance(
    party::PartyId party_id,
    const party::PartyService &party_service) {
    if (!party_service.getPartyInfo(party_id)) {
        return std::nullopt;
    }

    InstanceRecord record;
    record.id = next_instance_id_++;
    record.party_id = party_id;
    record.state = InstanceState::Waiting;
    instances_.emplace(record.id, record);
    return record.id;
}

bool InstanceManager::terminateInstance(InstanceId instance_id) {
    auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return false;
    }
    it->second.state = InstanceState::Terminate;
    return true;
}

bool InstanceManager::requestTransition(InstanceId instance_id,
                                        InstanceState next_state,
                                        const party::PartyService &party_service) {
    auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return false;
    }

    if (it->second.state == next_state) {
        return false;
    }

    if (!transitionAllowed(it->second.state, next_state)) {
        return false;
    }

    if (next_state == InstanceState::Ready || next_state == InstanceState::Playing) {
        if (!isPartyReady(it->second.party_id, party_service)) {
            return false;
        }
    }

    it->second.state = next_state;
    return true;
}

std::optional<InstanceRecord> InstanceManager::getInstance(
    InstanceId instance_id) const {
    auto it = instances_.find(instance_id);
    if (it == instances_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t InstanceManager::size() const {
    return instances_.size();
}

bool InstanceManager::isPartyReady(party::PartyId party_id,
                                   const party::PartyService &party_service) const {
    auto info = party_service.getPartyInfo(party_id);
    if (!info || info->members.empty()) {
        return false;
    }

    auto leader_present = std::any_of(info->members.begin(), info->members.end(),
                                      [&](const party::PartyMember &member) {
                                          return member.session_id == info->leader_session_id;
                                      });
    return leader_present;
}

bool InstanceManager::transitionAllowed(InstanceState from, InstanceState to) const {
    switch (from) {
        case InstanceState::Waiting:
            return to == InstanceState::Ready || to == InstanceState::Terminate;
        case InstanceState::Ready:
            return to == InstanceState::Playing || to == InstanceState::Terminate;
        case InstanceState::Playing:
            return to == InstanceState::Clear || to == InstanceState::Fail ||
                   to == InstanceState::Terminate;
        case InstanceState::Clear:
        case InstanceState::Fail:
            return to == InstanceState::Terminate;
        case InstanceState::Terminate:
            return false;
    }
    return false;
}

}  // namespace dungeon
