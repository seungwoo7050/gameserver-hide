#include "combat/dispatcher.h"
#include "reward/drop_table.h"
#include "reward/inventory.h"
#include "reward/reward_service.h"

#include <cassert>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

int main() {
    {
        combat::Dispatcher dispatcher;
        bool damage_seen = false;
        dispatcher.setDamageHandler([&damage_seen](const combat::DamageEvent &event) {
            damage_seen = true;
            assert(event.amount == 150);
        });

        combat::SkillEvent skill{10, 20, 99, 150};
        auto damage = dispatcher.processSkillEvent(skill);
        assert(damage.source_id == 10);
        assert(damage.target_id == 20);
        assert(damage.skill_id == 99);
        assert(damage.amount == 150);
        assert(damage_seen);
        assert(dispatcher.damageHistory().size() == 1);
    }

    {
        combat::Dispatcher dispatcher;
        dispatcher.setSkillHandler([](const combat::SkillEvent &event)
                                       -> std::optional<combat::DamageEvent> {
            combat::DamageEvent custom;
            custom.source_id = event.attacker_id;
            custom.target_id = event.target_id;
            custom.skill_id = event.skill_id;
            custom.amount = event.base_damage + 25;
            return custom;
        });

        combat::SkillEvent skill{1, 2, 3, 10};
        auto damage = dispatcher.processSkillEvent(skill);
        assert(damage.amount == 35);
        assert(dispatcher.damageHistory().size() == 1);
    }

    {
        reward::Inventory inventory(5);
        reward::RewardService service;
        std::vector<reward::RewardItem> items = {
            {1001, 2},
            {2002, 1}
        };
        assert(service.grantRewards(inventory, 1, items));
        assert(inventory.grantStatus(1) == reward::GrantStatus::Completed);
        assert(inventory.items().at(1001) == 2);
        assert(inventory.items().at(2002) == 1);

        assert(!service.grantRewards(inventory, 1, items));
        assert(inventory.grantStatus(1) == reward::GrantStatus::Completed);
        assert(inventory.items().at(1001) == 2);
        assert(inventory.items().at(2002) == 1);
    }

    {
        reward::Inventory inventory(2);
        reward::RewardService service;
        std::vector<reward::RewardItem> items = {
            {3003, 1},
            {4004, 2}
        };
        assert(!service.grantRewards(inventory, 55, items));
        assert(inventory.grantStatus(55) == reward::GrantStatus::Failed);
        assert(inventory.items().empty());
    }

    {
        reward::Inventory inventory(3);
        reward::RewardService service;
        std::mt19937 rng(7);
        auto rewards = service.dropTable().roll(1, rng);
        assert(service.grantRewards(inventory, 77, rewards));
        assert(inventory.grantStatus(77) == reward::GrantStatus::Completed);
        assert(inventory.totalQuantity() <= 3);
    }

    return 0;
}
