#include "reward/inventory.h"

#include <algorithm>

namespace reward {

Inventory::Inventory(std::size_t capacity) : capacity_(capacity) {}

bool Inventory::beginGrant(GrantId grant_id) {
    auto &status = grant_status_[grant_id];
    if (status == GrantStatus::Completed || status == GrantStatus::Pending) {
        return false;
    }
    status = GrantStatus::Pending;
    return true;
}

void Inventory::commitGrant(GrantId grant_id) {
    grant_status_[grant_id] = GrantStatus::Completed;
}

void Inventory::failGrant(GrantId grant_id) {
    grant_status_[grant_id] = GrantStatus::Failed;
}

bool Inventory::addItem(std::uint32_t item_id, std::uint32_t quantity) {
    if (quantity == 0) {
        return false;
    }
    const std::size_t current_total = totalQuantity();
    if (current_total + quantity > capacity_) {
        return false;
    }
    items_[item_id] += quantity;
    return true;
}

void Inventory::removeItem(std::uint32_t item_id, std::uint32_t quantity) {
    if (quantity == 0) {
        return;
    }
    auto it = items_.find(item_id);
    if (it == items_.end()) {
        return;
    }
    if (it->second <= quantity) {
        items_.erase(it);
    } else {
        it->second -= quantity;
    }
}

GrantStatus Inventory::grantStatus(GrantId grant_id) const {
    auto it = grant_status_.find(grant_id);
    if (it == grant_status_.end()) {
        return GrantStatus::None;
    }
    return it->second;
}

std::size_t Inventory::totalQuantity() const {
    std::size_t total = 0;
    for (const auto &entry : items_) {
        total += entry.second;
    }
    return total;
}

const std::unordered_map<std::uint32_t, std::uint32_t> &Inventory::items() const {
    return items_;
}

}  // namespace reward
