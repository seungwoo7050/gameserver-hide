#pragma once

#include "reward/drop_table.h"
#include "reward/inventory.h"

#include <cstdint>
#include <random>
#include <vector>

namespace reward {

class RewardService {
public:
    enum class GrantResult {
        Completed,
        Duplicate,
        Failed
    };

    GrantResult grantRewardsDetailed(Inventory &inventory,
                                     GrantId grant_id,
                                     const std::vector<RewardItem> &items);
    bool grantRewards(Inventory &inventory, GrantId grant_id, const std::vector<RewardItem> &items);
    bool grantFromTable(Inventory &inventory, GrantId grant_id, std::uint32_t table_id, std::mt19937 &rng);

    const DropTable &dropTable() const;
    DropTable &dropTable();

private:
    DropTable drop_table_{};
};

}  // namespace reward
