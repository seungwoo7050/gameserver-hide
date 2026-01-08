#pragma once

#include <optional>
#include <string>
#include <vector>

#include "inventory/inventory_models.h"

namespace inventory {

class InventoryStorage {
public:
    virtual ~InventoryStorage() = default;

    virtual Transaction beginTransaction() = 0;
    virtual void commitTransaction(const Transaction &transaction) = 0;
    virtual void rollbackTransaction(const Transaction &transaction) = 0;

    virtual std::optional<InventoryState> loadInventory(InventoryId inventory_id) const = 0;
    virtual void saveInventory(const InventoryState &state) = 0;

    virtual bool addItem(InventoryId inventory_id,
                         ItemId item_id,
                         Quantity quantity,
                         std::string reason) = 0;
    virtual bool removeItem(InventoryId inventory_id,
                            ItemId item_id,
                            Quantity quantity,
                            std::string reason) = 0;
    virtual void setItem(InventoryId inventory_id,
                         ItemId item_id,
                         Quantity quantity,
                         std::string reason) = 0;

    virtual std::vector<InventoryChange> changeLog(InventoryId inventory_id) const = 0;
};

}  // namespace inventory
