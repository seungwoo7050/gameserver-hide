#include "inventory/cached_inventory_storage.h"

namespace inventory {

CachedInventoryStorage::CachedInventoryStorage(std::unique_ptr<InventoryStorage> persistent,
                                               std::unique_ptr<InventoryStorage> cache)
    : persistent_(std::move(persistent)), cache_(std::move(cache)) {}

Transaction CachedInventoryStorage::beginTransaction() {
    Transaction transaction{next_transaction_id_++};
    TransactionPair pair{persistent_->beginTransaction(), cache_->beginTransaction()};
    transactions_.emplace(transaction.transaction_id, pair);
    return transaction;
}

void CachedInventoryStorage::commitTransaction(const Transaction &transaction) {
    auto it = transactions_.find(transaction.transaction_id);
    if (it == transactions_.end()) {
        return;
    }
    persistent_->commitTransaction(it->second.persistent);
    cache_->commitTransaction(it->second.cache);
    transactions_.erase(it);
}

void CachedInventoryStorage::rollbackTransaction(const Transaction &transaction) {
    auto it = transactions_.find(transaction.transaction_id);
    if (it == transactions_.end()) {
        return;
    }
    persistent_->rollbackTransaction(it->second.persistent);
    cache_->rollbackTransaction(it->second.cache);
    transactions_.erase(it);
}

std::optional<InventoryState> CachedInventoryStorage::loadInventory(
    InventoryId inventory_id) const {
    auto cached = cache_->loadInventory(inventory_id);
    if (cached) {
        return cached;
    }
    auto persisted = persistent_->loadInventory(inventory_id);
    if (persisted) {
        cache_->saveInventory(*persisted);
    }
    return persisted;
}

void CachedInventoryStorage::saveInventory(const InventoryState &state) {
    persistent_->saveInventory(state);
    cache_->saveInventory(state);
}

bool CachedInventoryStorage::addItem(InventoryId inventory_id,
                                     ItemId item_id,
                                     Quantity quantity,
                                     std::string reason) {
    if (!persistent_->addItem(inventory_id, item_id, quantity, reason)) {
        return false;
    }
    if (!cache_->addItem(inventory_id, item_id, quantity, reason)) {
        refreshCache(inventory_id);
    }
    return true;
}

bool CachedInventoryStorage::removeItem(InventoryId inventory_id,
                                        ItemId item_id,
                                        Quantity quantity,
                                        std::string reason) {
    if (!persistent_->removeItem(inventory_id, item_id, quantity, reason)) {
        return false;
    }
    if (!cache_->removeItem(inventory_id, item_id, quantity, reason)) {
        refreshCache(inventory_id);
    }
    return true;
}

void CachedInventoryStorage::setItem(InventoryId inventory_id,
                                     ItemId item_id,
                                     Quantity quantity,
                                     std::string reason) {
    persistent_->setItem(inventory_id, item_id, quantity, reason);
    cache_->setItem(inventory_id, item_id, quantity, reason);
}

std::vector<InventoryChange> CachedInventoryStorage::changeLog(
    InventoryId inventory_id) const {
    return persistent_->changeLog(inventory_id);
}

void CachedInventoryStorage::refreshCache(InventoryId inventory_id) const {
    auto persisted = persistent_->loadInventory(inventory_id);
    if (persisted) {
        cache_->saveInventory(*persisted);
    }
}

}  // namespace inventory
