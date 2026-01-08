#include "inventory/in_memory_inventory_storage.h"

#include <cassert>
#include <string>

int main() {
    {
        inventory::InMemoryInventoryStorage storage;
        const inventory::InventoryId inventory_id = 10;

        assert(!storage.addItem(inventory_id, 1001, 0, "zero add"));
        assert(storage.addItem(inventory_id, 1001, 3, "loot"));
        assert(storage.addItem(inventory_id, 1002, 1, "loot"));
        assert(storage.removeItem(inventory_id, 1001, 1, "use"));
        assert(!storage.removeItem(inventory_id, 1001, 5, "too much"));
        storage.setItem(inventory_id, 1003, 7, "set");

        auto state = storage.loadInventory(inventory_id);
        assert(state.has_value());
        assert(state->items.at(1001) == 2);
        assert(state->items.at(1002) == 1);
        assert(state->items.at(1003) == 7);
    }

    {
        inventory::InMemoryInventoryStorage storage;
        const inventory::InventoryId inventory_id = 20;

        inventory::InventoryState saved;
        saved.inventory_id = inventory_id;
        saved.items[2001] = 4;
        saved.items[2002] = 2;
        storage.saveInventory(saved);

        auto state = storage.loadInventory(inventory_id);
        assert(state.has_value());
        assert(state->items.size() == 2);
        assert(state->items.at(2001) == 4);
        assert(state->items.at(2002) == 2);
    }

    {
        inventory::InMemoryInventoryStorage storage;
        const inventory::InventoryId inventory_id = 30;

        auto transaction = storage.beginTransaction();
        assert(storage.addItem(inventory_id, 3001, 5, "tx add"));
        storage.setItem(inventory_id, 3002, 1, "tx set");
        storage.rollbackTransaction(transaction);

        auto state = storage.loadInventory(inventory_id);
        assert(!state.has_value() || state->items.empty());
        assert(storage.changeLog(inventory_id).empty());

        auto transaction2 = storage.beginTransaction();
        assert(storage.addItem(inventory_id, 3001, 2, "tx commit"));
        storage.commitTransaction(transaction2);

        auto committed = storage.loadInventory(inventory_id);
        assert(committed.has_value());
        assert(committed->items.at(3001) == 2);
    }

    {
        inventory::InMemoryInventoryStorage storage;
        const inventory::InventoryId inventory_id = 40;

        assert(storage.addItem(inventory_id, 4001, 1, "reward"));
        assert(storage.removeItem(inventory_id, 4001, 1, "consume"));
        storage.setItem(inventory_id, 4002, 3, "set count");

        auto log = storage.changeLog(inventory_id);
        assert(log.size() == 3);
        assert(log[0].inventory_id == inventory_id);
        assert(log[0].item_id == 4001);
        assert(log[0].quantity == 1);
        assert(log[0].type == inventory::ChangeType::Add);
        assert(log[0].reason == "reward");

        assert(log[1].type == inventory::ChangeType::Remove);
        assert(log[1].reason == "consume");

        assert(log[2].item_id == 4002);
        assert(log[2].quantity == 3);
        assert(log[2].type == inventory::ChangeType::Set);
        assert(log[2].reason == "set count");
    }

    return 0;
}
