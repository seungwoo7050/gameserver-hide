#include "inventory/cached_inventory_storage.h"
#include "inventory/in_memory_inventory_storage.h"
#include "inventory/mysql_inventory_storage.h"

#include <cassert>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

class CountingStorage : public inventory::InventoryStorage {
public:
    struct Counters {
        int begin{0};
        int commit{0};
        int rollback{0};
        int load{0};
        int save{0};
        int add{0};
        int remove{0};
        int set{0};
    };

    explicit CountingStorage(bool fail_add = false, bool fail_remove = false)
        : fail_add_(fail_add), fail_remove_(fail_remove) {}

    mutable Counters counters;

    inventory::Transaction beginTransaction() override {
        counters.begin += 1;
        return inventory::Transaction{next_transaction_id_++};
    }

    void commitTransaction(const inventory::Transaction &) override {
        counters.commit += 1;
    }

    void rollbackTransaction(const inventory::Transaction &) override {
        counters.rollback += 1;
    }

    std::optional<inventory::InventoryState> loadInventory(
        inventory::InventoryId inventory_id) const override {
        counters.load += 1;
        auto it = inventories_.find(inventory_id);
        if (it == inventories_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void saveInventory(const inventory::InventoryState &state) override {
        counters.save += 1;
        inventories_[state.inventory_id] = state;
    }

    bool addItem(inventory::InventoryId inventory_id,
                 inventory::ItemId item_id,
                 inventory::Quantity quantity,
                 std::string reason) override {
        counters.add += 1;
        if (fail_add_ || quantity == 0) {
            return false;
        }
        auto &inventory = inventories_[inventory_id];
        inventory.inventory_id = inventory_id;
        inventory.items[item_id] += quantity;
        recordChange(inventory_id, item_id, quantity, inventory::ChangeType::Add,
                     std::move(reason));
        return true;
    }

    bool removeItem(inventory::InventoryId inventory_id,
                    inventory::ItemId item_id,
                    inventory::Quantity quantity,
                    std::string reason) override {
        counters.remove += 1;
        if (fail_remove_ || quantity == 0) {
            return false;
        }
        auto &inventory = inventories_[inventory_id];
        auto it = inventory.items.find(item_id);
        if (it == inventory.items.end() || it->second < quantity) {
            return false;
        }
        it->second -= quantity;
        if (it->second == 0) {
            inventory.items.erase(it);
        }
        recordChange(inventory_id, item_id, quantity, inventory::ChangeType::Remove,
                     std::move(reason));
        return true;
    }

    void setItem(inventory::InventoryId inventory_id,
                 inventory::ItemId item_id,
                 inventory::Quantity quantity,
                 std::string reason) override {
        counters.set += 1;
        auto &inventory = inventories_[inventory_id];
        inventory.inventory_id = inventory_id;
        if (quantity == 0) {
            inventory.items.erase(item_id);
        } else {
            inventory.items[item_id] = quantity;
        }
        recordChange(inventory_id, item_id, quantity, inventory::ChangeType::Set,
                     std::move(reason));
    }

    std::vector<inventory::InventoryChange> changeLog(
        inventory::InventoryId inventory_id) const override {
        auto it = change_log_.find(inventory_id);
        if (it == change_log_.end()) {
            return {};
        }
        return it->second;
    }

private:
    void recordChange(inventory::InventoryId inventory_id,
                      inventory::ItemId item_id,
                      inventory::Quantity quantity,
                      inventory::ChangeType type,
                      std::string reason) const {
        inventory::InventoryChange change;
        change.change_id = next_change_id_++;
        change.inventory_id = inventory_id;
        change.item_id = item_id;
        change.quantity = quantity;
        change.type = type;
        change.reason = std::move(reason);
        change_log_[inventory_id].push_back(std::move(change));
    }

    bool fail_add_{false};
    bool fail_remove_{false};
    mutable inventory::ChangeId next_change_id_{1};
    inventory::TransactionId next_transaction_id_{1};
    mutable std::unordered_map<inventory::InventoryId, inventory::InventoryState>
        inventories_;
    mutable std::unordered_map<inventory::InventoryId,
                               std::vector<inventory::InventoryChange>>
        change_log_;
};

}  // namespace

int main() {
    {
        inventory::CachedInventoryStorage storage(
            std::make_unique<inventory::MySqlInventoryStorage>(),
            std::make_unique<inventory::InMemoryInventoryStorage>());
        const inventory::InventoryId inventory_id = 101;

        assert(storage.addItem(inventory_id, 1001, 2, "loot"));
        assert(storage.addItem(inventory_id, 1002, 3, "loot"));
        assert(storage.removeItem(inventory_id, 1001, 1, "consume"));
        storage.setItem(inventory_id, 1003, 5, "set");

        auto state = storage.loadInventory(inventory_id);
        assert(state.has_value());
        assert(state->items.at(1001) == 1);
        assert(state->items.at(1002) == 3);
        assert(state->items.at(1003) == 5);
    }

    {
        inventory::CachedInventoryStorage storage(
            std::make_unique<inventory::MySqlInventoryStorage>(),
            std::make_unique<inventory::InMemoryInventoryStorage>());
        const inventory::InventoryId inventory_id = 202;

        auto tx = storage.beginTransaction();
        assert(storage.addItem(inventory_id, 2001, 4, "reward"));
        storage.rollbackTransaction(tx);

        auto state = storage.loadInventory(inventory_id);
        assert(!state.has_value() || state->items.empty());

        auto tx2 = storage.beginTransaction();
        assert(storage.addItem(inventory_id, 2001, 2, "commit"));
        storage.commitTransaction(tx2);

        auto committed = storage.loadInventory(inventory_id);
        assert(committed.has_value());
        assert(committed->items.at(2001) == 2);
    }

    {
        auto persistent = std::make_unique<CountingStorage>();
        auto cache = std::make_unique<CountingStorage>();
        auto *persistent_ptr = persistent.get();
        auto *cache_ptr = cache.get();
        const inventory::InventoryId inventory_id = 303;

        inventory::InventoryState state;
        state.inventory_id = inventory_id;
        state.items[3001] = 1;
        persistent_ptr->saveInventory(state);

        inventory::CachedInventoryStorage storage(std::move(persistent), std::move(cache));

        auto loaded = storage.loadInventory(inventory_id);
        assert(loaded.has_value());
        assert(loaded->items.at(3001) == 1);
        assert(persistent_ptr->counters.load == 1);
        assert(cache_ptr->counters.save == 1);

        auto second = storage.loadInventory(inventory_id);
        assert(second.has_value());
        assert(persistent_ptr->counters.load == 1);
        assert(cache_ptr->counters.load >= 2);
    }

    {
        auto persistent = std::make_unique<CountingStorage>();
        auto cache = std::make_unique<CountingStorage>(true, false);
        auto *cache_ptr = cache.get();
        const inventory::InventoryId inventory_id = 404;

        inventory::CachedInventoryStorage storage(std::move(persistent), std::move(cache));

        assert(storage.addItem(inventory_id, 4001, 2, "grant"));
        auto refreshed = storage.loadInventory(inventory_id);
        assert(refreshed.has_value());
        assert(refreshed->items.at(4001) == 2);
        assert(cache_ptr->counters.save >= 1);
    }

    {
        inventory::CachedInventoryStorage storage(
            std::make_unique<inventory::MySqlInventoryStorage>(),
            std::make_unique<inventory::InMemoryInventoryStorage>());
        const inventory::InventoryId inventory_id = 505;

        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&storage, inventory_id, i]() {
                for (int j = 0; j < 50; ++j) {
                    storage.addItem(inventory_id, 5001, 1, "concurrent");
                    if (j % 10 == 0) {
                        storage.setItem(inventory_id, 5002,
                                        static_cast<inventory::Quantity>(i + j),
                                        "overwrite");
                    }
                }
            });
        }
        for (auto &thread : threads) {
            thread.join();
        }

        auto state = storage.loadInventory(inventory_id);
        assert(state.has_value());
        assert(state->items.at(5001) == 8 * 50);
    }

    return 0;
}
