#include "reward/drop_table.h"

#include <algorithm>

namespace reward {

DropTable::DropTable() {
    addEntry(1, DropEntry{1001, 1, 2, 0.75f});
    addEntry(1, DropEntry{2001, 1, 1, 0.25f});
    addEntry(1, DropEntry{3001, 2, 4, 0.10f});
}

void DropTable::addEntry(std::uint32_t table_id, const DropEntry &entry) {
    tables_[table_id].push_back(entry);
}

bool DropTable::hasTable(std::uint32_t table_id) const {
    return tables_.find(table_id) != tables_.end();
}

std::vector<RewardItem> DropTable::roll(std::uint32_t table_id, std::mt19937 &rng) const {
    std::vector<RewardItem> rewards;
    auto it = tables_.find(table_id);
    if (it == tables_.end()) {
        return rewards;
    }

    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    for (const auto &entry : it->second) {
        if (chance(rng) > entry.probability) {
            continue;
        }
        std::uint32_t min_qty = std::min(entry.min_quantity, entry.max_quantity);
        std::uint32_t max_qty = std::max(entry.min_quantity, entry.max_quantity);
        std::uniform_int_distribution<std::uint32_t> quantity(min_qty, max_qty);
        rewards.push_back(RewardItem{entry.item_id, quantity(rng)});
    }

    return rewards;
}

}  // namespace reward
