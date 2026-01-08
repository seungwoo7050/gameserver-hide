#include "match/match_queue.h"
#include "party/party.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

int main() {
    using namespace std::chrono;

    {
        party::PartyService service;
        std::vector<std::tuple<party::SessionId, party::PartyEventType>> events;
        service.setEventSink([&](party::SessionId session_id, const party::PartyEvent &event) {
            events.emplace_back(session_id, event.type);
        });
        auto party_id = service.createParty(1, "leader");
        assert(party_id.has_value());
        auto now = steady_clock::now();
        assert(service.inviteMember(*party_id, 1, 2, "member", now));
        assert(service.acceptInvite(*party_id, 2, now + seconds{1}));
        auto info = service.getPartyInfo(*party_id);
        assert(info.has_value());
        assert(info->members.size() == 2);
        assert(service.disbandParty(*party_id, 1));
        assert(!service.getPartyInfo(*party_id).has_value());
        assert(!events.empty());
        assert(std::get<1>(events.front()) == party::PartyEventType::Created);
    }

    {
        party::PartyService service;
        std::vector<party::PartyEventType> events;
        service.setEventSink([&](party::SessionId, const party::PartyEvent &event) {
            events.push_back(event.type);
        });
        service.setInviteTimeout(milliseconds{5});
        auto party_id = service.createParty(10, "leader");
        assert(party_id.has_value());
        auto now = steady_clock::now();
        assert(service.inviteMember(*party_id, 10, 20, "member", now));
        assert(service.rejectInvite(*party_id, 20));
        assert(!service.acceptInvite(*party_id, 20, now + milliseconds{1}));
        assert(std::find(events.begin(), events.end(),
                         party::PartyEventType::InviteRejected) != events.end());
    }

    {
        party::PartyService service;
        std::vector<party::PartyEventType> events;
        service.setEventSink([&](party::SessionId, const party::PartyEvent &event) {
            events.push_back(event.type);
        });
        service.setInviteTimeout(milliseconds{5});
        auto party_id = service.createParty(100, "leader");
        assert(party_id.has_value());
        auto now = steady_clock::now();
        assert(service.inviteMember(*party_id, 100, 200, "member", now));
        auto expired = service.expireInvites(now + milliseconds{10});
        assert(expired == 1);
        assert(!service.acceptInvite(*party_id, 200, now + milliseconds{10}));
        assert(std::find(events.begin(), events.end(),
                         party::PartyEventType::InviteExpired) != events.end());
    }

    {
        match::MatchRule rule;
        rule.max_mmr_delta = 100;
        rule.expansion_per_second = 10;
        rule.min_party_size = 1;
        rule.max_party_size = 5;
        match::MatchQueue queue(rule);
        auto now = steady_clock::now();
        match::MatchCandidate first{1, 1000, 2, now};
        match::MatchCandidate second{2, 1050, 2, now};
        assert(queue.enqueue(first));
        assert(queue.enqueue(second));
        auto match_pair = queue.findMatch(now + seconds{1});
        assert(match_pair.has_value());
        assert(queue.size() == 0);
    }

    {
        match::MatchRule rule;
        match::MatchQueue queue(rule);
        auto now = steady_clock::now();
        match::MatchCandidate candidate{42, 900, 1, now};
        assert(queue.enqueue(candidate));
        assert(queue.cancel(42));
        assert(queue.size() == 0);
    }

    {
        match::MatchRule rule;
        rule.min_party_size = 1;
        match::MatchQueue queue(rule);
        auto now = steady_clock::now();
        match::MatchCandidate candidate{77, 1200, 2, now};
        assert(queue.enqueue(candidate));
        assert(queue.updatePartySize(77, 0, now + seconds{1}));
        assert(queue.size() == 0);
    }

    return 0;
}
