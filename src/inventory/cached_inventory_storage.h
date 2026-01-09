#pragma once

#include <memory>
#include <unordered_map>

#include "inventory/inventory_storage.h"

namespace inventory {

class CachedInventoryStorage : public InventoryStorage {
public:
    CachedInventoryStorage(std::unique_ptr<InventoryStorage> persistent,
                           std::unique_ptr<InventoryStorage> cache);

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
    struct TransactionPair {
        Transaction persistent;
        Transaction cache;
    };

    void refreshCache(InventoryId inventory_id) const;

    TransactionId next_transaction_id_{1};
    std::unordered_map<TransactionId, TransactionPair> transactions_;
    std::unique_ptr<InventoryStorage> persistent_;
    std::unique_ptr<InventoryStorage> cache_;
};

}  // namespace inventory
