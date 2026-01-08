#include "reward/inventory.h"
#include "reward/reward_service.h"

#include <cassert>
#include <vector>

int main() {
    {
        reward::Inventory inventory(5);
        reward::RewardService service;
        std::vector<reward::RewardItem> items = {
            {101, 2},
            {202, 1}
        };

        auto first = service.grantRewardsDetailed(inventory, 100, items);
        assert(first == reward::RewardService::GrantResult::Completed);
        assert(inventory.items().at(101) == 2);
        assert(inventory.items().at(202) == 1);

        auto duplicate = service.grantRewardsDetailed(inventory, 100, items);
        assert(duplicate == reward::RewardService::GrantResult::Duplicate);
        assert(inventory.grantStatus(100) == reward::GrantStatus::Completed);
        assert(inventory.items().at(101) == 2);
        assert(inventory.items().at(202) == 1);
    }

    {
        reward::Inventory inventory(2);
        reward::RewardService service;
        std::vector<reward::RewardItem> items = {
            {303, 1},
            {404, 2}
        };

        auto failed = service.grantRewardsDetailed(inventory, 200, items);
        assert(failed == reward::RewardService::GrantResult::Failed);
        assert(inventory.grantStatus(200) == reward::GrantStatus::Failed);
        assert(inventory.items().empty());
    }

    return 0;
}
