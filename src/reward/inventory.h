#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace reward {

using GrantId = std::uint64_t;

enum class GrantStatus {
    None,
    Pending,
    Completed,
    Failed
};

class Inventory {
public:
    explicit Inventory(std::size_t capacity = 100);

    bool beginGrant(GrantId grant_id);
    void commitGrant(GrantId grant_id);
    void failGrant(GrantId grant_id);

    bool addItem(std::uint32_t item_id, std::uint32_t quantity);
    void removeItem(std::uint32_t item_id, std::uint32_t quantity);

    GrantStatus grantStatus(GrantId grant_id) const;
    std::size_t totalQuantity() const;
    const std::unordered_map<std::uint32_t, std::uint32_t> &items() const;

private:
    std::size_t capacity_;
    std::unordered_map<std::uint32_t, std::uint32_t> items_;
    std::unordered_map<GrantId, GrantStatus> grant_status_;
};

}  // namespace reward
