#include "inventory/in_memory_inventory_storage.h"

#include <chrono>

namespace inventory {

Transaction InMemoryInventoryStorage::beginTransaction() {
    Transaction transaction{next_transaction_id_++};
    active_transactions_.insert(transaction.transaction_id);
    TransactionSnapshot snapshot;
    snapshot.inventories = inventories_;
    snapshot.change_log = change_log_;
    snapshot.next_change_id = next_change_id_;
    transaction_snapshots_.emplace(transaction.transaction_id, std::move(snapshot));
    return transaction;
}

void InMemoryInventoryStorage::commitTransaction(const Transaction &transaction) {
    active_transactions_.erase(transaction.transaction_id);
    transaction_snapshots_.erase(transaction.transaction_id);
}

void InMemoryInventoryStorage::rollbackTransaction(const Transaction &transaction) {
    auto snapshot_it = transaction_snapshots_.find(transaction.transaction_id);
    if (snapshot_it != transaction_snapshots_.end()) {
        inventories_ = snapshot_it->second.inventories;
        change_log_ = snapshot_it->second.change_log;
        next_change_id_ = snapshot_it->second.next_change_id;
        transaction_snapshots_.erase(snapshot_it);
    }
    active_transactions_.erase(transaction.transaction_id);
}

std::optional<InventoryState> InMemoryInventoryStorage::loadInventory(InventoryId inventory_id) const {
    auto it = inventories_.find(inventory_id);
    if (it == inventories_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void InMemoryInventoryStorage::saveInventory(const InventoryState &state) {
    inventories_[state.inventory_id] = state;
}

bool InMemoryInventoryStorage::addItem(InventoryId inventory_id,
                                       ItemId item_id,
                                       Quantity quantity,
                                       std::string reason) {
    if (quantity == 0) {
        return false;
    }

    auto &inventory = getOrCreateInventory(inventory_id);
    inventory.items[item_id] += quantity;
    recordChange(inventory_id, item_id, quantity, ChangeType::Add, std::move(reason));
    return true;
}

bool InMemoryInventoryStorage::removeItem(InventoryId inventory_id,
                                          ItemId item_id,
                                          Quantity quantity,
                                          std::string reason) {
    if (quantity == 0) {
        return false;
    }

    auto &inventory = getOrCreateInventory(inventory_id);
    auto it = inventory.items.find(item_id);
    if (it == inventory.items.end() || it->second < quantity) {
        return false;
    }

    it->second -= quantity;
    if (it->second == 0) {
        inventory.items.erase(it);
    }
    recordChange(inventory_id, item_id, quantity, ChangeType::Remove, std::move(reason));
    return true;
}

void InMemoryInventoryStorage::setItem(InventoryId inventory_id,
                                       ItemId item_id,
                                       Quantity quantity,
                                       std::string reason) {
    auto &inventory = getOrCreateInventory(inventory_id);
    if (quantity == 0) {
        inventory.items.erase(item_id);
    } else {
        inventory.items[item_id] = quantity;
    }
    recordChange(inventory_id, item_id, quantity, ChangeType::Set, std::move(reason));
}

std::vector<InventoryChange> InMemoryInventoryStorage::changeLog(InventoryId inventory_id) const {
    auto it = change_log_.find(inventory_id);
    if (it == change_log_.end()) {
        return {};
    }
    return it->second;
}

InventoryState &InMemoryInventoryStorage::getOrCreateInventory(InventoryId inventory_id) {
    auto [it, inserted] = inventories_.try_emplace(inventory_id, InventoryState{inventory_id});
    return it->second;
}

void InMemoryInventoryStorage::recordChange(InventoryId inventory_id,
                                            ItemId item_id,
                                            Quantity quantity,
                                            ChangeType type,
                                            std::string reason) {
    InventoryChange change;
    change.change_id = next_change_id_++;
    change.inventory_id = inventory_id;
    change.item_id = item_id;
    change.quantity = quantity;
    change.type = type;
    change.reason = std::move(reason);
    change.recorded_at = std::chrono::system_clock::now();
    change_log_[inventory_id].push_back(std::move(change));
}

}  // namespace inventory
