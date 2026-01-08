#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace inventory {

using InventoryId = std::uint64_t;
using ItemId = std::uint32_t;
using Quantity = std::uint32_t;
using ChangeId = std::uint64_t;
using TransactionId = std::uint64_t;

struct ItemStack {
    ItemId item_id{0};
    Quantity quantity{0};
};

struct InventoryState {
    InventoryId inventory_id{0};
    std::unordered_map<ItemId, Quantity> items;
};

enum class ChangeType : std::uint8_t {
    Add,
    Remove,
    Set
};

struct InventoryChange {
    ChangeId change_id{0};
    InventoryId inventory_id{0};
    ItemId item_id{0};
    Quantity quantity{0};
    ChangeType type{ChangeType::Add};
    std::string reason;
    std::chrono::system_clock::time_point recorded_at{};
};

struct Transaction {
    TransactionId transaction_id{0};
};

}  // namespace inventory
