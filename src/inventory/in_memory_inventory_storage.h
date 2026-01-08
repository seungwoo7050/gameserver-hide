#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "inventory/inventory_storage.h"

namespace inventory {

class InMemoryInventoryStorage : public InventoryStorage {
public:
    Transaction beginTransaction() override;
    void commitTransaction(const Transaction &transaction) override;
    void rollbackTransaction(const Transaction &transaction) override;

    std::optional<InventoryState> loadInventory(InventoryId inventory_id) const override;
    void saveInventory(const InventoryState &state) override;

    bool addItem(InventoryId inventory_id,
                 ItemId item_id,
                 Quantity quantity,
                 std::string reason) override;
    bool removeItem(InventoryId inventory_id,
                    ItemId item_id,
                    Quantity quantity,
                    std::string reason) override;
    void setItem(InventoryId inventory_id,
                 ItemId item_id,
                 Quantity quantity,
                 std::string reason) override;

    std::vector<InventoryChange> changeLog(InventoryId inventory_id) const override;

private:
    InventoryState &getOrCreateInventory(InventoryId inventory_id);
    void recordChange(InventoryId inventory_id,
                      ItemId item_id,
                      Quantity quantity,
                      ChangeType type,
                      std::string reason);

    struct TransactionSnapshot {
        std::unordered_map<InventoryId, InventoryState> inventories;
        std::unordered_map<InventoryId, std::vector<InventoryChange>> change_log;
        ChangeId next_change_id{1};
    };

    TransactionId next_transaction_id_{1};
    ChangeId next_change_id_{1};
    std::unordered_set<TransactionId> active_transactions_;
    std::unordered_map<TransactionId, TransactionSnapshot> transaction_snapshots_;
    std::unordered_map<InventoryId, InventoryState> inventories_;
    std::unordered_map<InventoryId, std::vector<InventoryChange>> change_log_;
};

}  // namespace inventory
