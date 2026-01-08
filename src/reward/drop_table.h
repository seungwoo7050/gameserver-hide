#pragma once

#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>

namespace reward {

struct RewardItem {
    std::uint32_t item_id{0};
    std::uint32_t quantity{0};
};

struct DropEntry {
    std::uint32_t item_id{0};
    std::uint32_t min_quantity{0};
    std::uint32_t max_quantity{0};
    float probability{0.0f};
};

class DropTable {
public:
    DropTable();

    void addEntry(std::uint32_t table_id, const DropEntry &entry);
    bool hasTable(std::uint32_t table_id) const;
    std::vector<RewardItem> roll(std::uint32_t table_id, std::mt19937 &rng) const;

private:
    std::unordered_map<std::uint32_t, std::vector<DropEntry>> tables_;
};

}  // namespace reward
