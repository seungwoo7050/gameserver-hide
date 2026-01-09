#include "dungeon/authoritative_validation.h"
#include "dungeon/instance_manager.h"
#include "party/party.h"

#include <cassert>
#include <chrono>

int main() {
    using namespace std::chrono;

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(1, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());
        assert(manager.size() == 1);

        auto record = manager.getInstance(*instance_id);
        assert(record.has_value());
        assert(record->state == dungeon::InstanceState::Waiting);

        assert(manager.terminateInstance(*instance_id));
        auto terminated = manager.getInstance(*instance_id);
        assert(terminated.has_value());
        assert(terminated->state == dungeon::InstanceState::Terminate);
        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Ready,
                                          party_service));
    }

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(10, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());

        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Ready,
                                         party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Playing,
                                         party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Clear,
                                         party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Terminate,
                                         party_service));
    }

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(15, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());

        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Playing,
                                          party_service));
    }

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(20, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());

        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Ready,
                                         party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Playing,
                                         party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Clear,
                                         party_service));
        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Ready,
                                          party_service));
        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Playing,
                                          party_service));
    }

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(100, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());

        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Playing,
                                          party_service));
        assert(manager.requestTransition(*instance_id,
                                         dungeon::InstanceState::Ready,
                                         party_service));
        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Waiting,
                                          party_service));
    }

    {
        party::PartyService party_service;
        auto party_id = party_service.createParty(200, "leader");
        assert(party_id.has_value());

        dungeon::InstanceManager manager;
        auto instance_id = manager.createInstance(*party_id, party_service);
        assert(instance_id.has_value());

        assert(party_service.disbandParty(*party_id, 200));
        assert(!manager.requestTransition(*instance_id,
                                          dungeon::InstanceState::Ready,
                                          party_service));
    }

    {
        dungeon::MovementValidator validator(5.0f);
        dungeon::MovementSample valid{1, 4.0f, milliseconds{1000}};
        std::string reason;
        assert(validator.validate(valid, reason));

        dungeon::MovementSample invalid{1, 20.0f, milliseconds{1000}};
        assert(!validator.validate(invalid, reason));
        assert(reason == "Movement speed exceeds server limit");
    }

    return 0;
}
