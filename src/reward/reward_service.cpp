#include "reward/reward_service.h"

namespace reward {

RewardService::GrantResult RewardService::grantRewardsDetailed(Inventory &inventory,
                                                               GrantId grant_id,
                                                               const std::vector<RewardItem> &items) {
    if (!inventory.beginGrant(grant_id)) {
        return GrantResult::Duplicate;
    }

    std::vector<RewardItem> applied;
    for (const auto &item : items) {
        if (!inventory.addItem(item.item_id, item.quantity)) {
            for (const auto &rollback : applied) {
                inventory.removeItem(rollback.item_id, rollback.quantity);
            }
            inventory.failGrant(grant_id);
            return GrantResult::Failed;
        }
        applied.push_back(item);
    }

    inventory.commitGrant(grant_id);
    return GrantResult::Completed;
}

bool RewardService::grantRewards(Inventory &inventory, GrantId grant_id, const std::vector<RewardItem> &items) {
    return grantRewardsDetailed(inventory, grant_id, items) == GrantResult::Completed;
}

bool RewardService::grantFromTable(Inventory &inventory, GrantId grant_id, std::uint32_t table_id, std::mt19937 &rng) {
    const auto rewards = drop_table_.roll(table_id, rng);
    return grantRewards(inventory, grant_id, rewards);
}

const DropTable &RewardService::dropTable() const {
    return drop_table_;
}

DropTable &RewardService::dropTable() {
    return drop_table_;
}

}  // namespace reward
