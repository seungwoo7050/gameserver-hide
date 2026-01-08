#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "party/party.h"

namespace dungeon {

using InstanceId = std::uint64_t;

enum class InstanceState : std::uint8_t {
    Waiting = 0,
    Ready = 1,
    Playing = 2,
    Clear = 3,
    Fail = 4,
    Terminate = 5
};

struct InstanceRecord {
    InstanceId id{0};
    party::PartyId party_id{0};
    InstanceState state{InstanceState::Waiting};
};

class InstanceManager {
public:
    InstanceManager();

    std::optional<InstanceId> createInstance(party::PartyId party_id,
                                             const party::PartyService &party_service);
    bool terminateInstance(InstanceId instance_id);
    bool requestTransition(InstanceId instance_id,
                           InstanceState next_state,
                           const party::PartyService &party_service);

    std::optional<InstanceRecord> getInstance(InstanceId instance_id) const;
    std::size_t size() const;

private:
    bool isPartyReady(party::PartyId party_id,
                      const party::PartyService &party_service) const;
    bool transitionAllowed(InstanceState from, InstanceState to) const;

    InstanceId next_instance_id_{1};
    std::unordered_map<InstanceId, InstanceRecord> instances_;
};

}  // namespace dungeon
