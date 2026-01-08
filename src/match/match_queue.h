#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace match {

struct MatchRule {
    int max_mmr_delta{100};
    int expansion_per_second{5};
    std::size_t min_party_size{1};
    std::size_t max_party_size{5};
};

struct MatchCandidate {
    std::uint64_t party_id{0};
    int mmr{0};
    std::size_t party_size{1};
    std::chrono::steady_clock::time_point enqueue_time;
};

class MatchQueue {
public:
    explicit MatchQueue(MatchRule rule);

    bool enqueue(MatchCandidate candidate);
    bool cancel(std::uint64_t party_id);
    bool updatePartySize(std::uint64_t party_id,
                         std::size_t party_size,
                         std::chrono::steady_clock::time_point now);
    std::optional<std::pair<MatchCandidate, MatchCandidate>>
    findMatch(std::chrono::steady_clock::time_point now);

    std::size_t size() const;

private:
    bool compatible(const MatchCandidate &first,
                    const MatchCandidate &second,
                    std::chrono::steady_clock::time_point now) const;

    MatchRule rule_;
    std::vector<MatchCandidate> queue_;
};

}  // namespace match
