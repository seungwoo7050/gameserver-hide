#include "match/match_queue.h"

#include <algorithm>
#include <cstdlib>

namespace match {

MatchQueue::MatchQueue(MatchRule rule) : rule_(rule) {}

bool MatchQueue::enqueue(MatchCandidate candidate) {
    if (candidate.party_id == 0) {
        return false;
    }
    if (candidate.party_size < rule_.min_party_size ||
        candidate.party_size > rule_.max_party_size) {
        return false;
    }

    auto existing = std::find_if(queue_.begin(), queue_.end(),
                                 [&](const MatchCandidate &entry) {
                                     return entry.party_id == candidate.party_id;
                                 });
    if (existing != queue_.end()) {
        *existing = candidate;
        return true;
    }

    queue_.push_back(candidate);
    return true;
}

bool MatchQueue::cancel(std::uint64_t party_id) {
    auto it = std::remove_if(queue_.begin(), queue_.end(),
                             [&](const MatchCandidate &entry) {
                                 return entry.party_id == party_id;
                             });
    if (it == queue_.end()) {
        return false;
    }
    queue_.erase(it, queue_.end());
    return true;
}

bool MatchQueue::updatePartySize(std::uint64_t party_id,
                                 std::size_t party_size,
                                 std::chrono::steady_clock::time_point now) {
    auto it = std::find_if(queue_.begin(), queue_.end(),
                           [&](const MatchCandidate &entry) {
                               return entry.party_id == party_id;
                           });
    if (it == queue_.end()) {
        return false;
    }
    if (party_size < rule_.min_party_size || party_size > rule_.max_party_size) {
        queue_.erase(it);
        return true;
    }
    it->party_size = party_size;
    it->enqueue_time = now;
    return true;
}

std::optional<std::pair<MatchCandidate, MatchCandidate>>
MatchQueue::findMatch(std::chrono::steady_clock::time_point now) {
    for (std::size_t i = 0; i < queue_.size(); ++i) {
        for (std::size_t j = i + 1; j < queue_.size(); ++j) {
            if (compatible(queue_[i], queue_[j], now)) {
                MatchCandidate first = queue_[i];
                MatchCandidate second = queue_[j];
                if (j > i) {
                    queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(j));
                }
                queue_.erase(queue_.begin() + static_cast<std::ptrdiff_t>(i));
                return std::make_pair(first, second);
            }
        }
    }
    return std::nullopt;
}

std::size_t MatchQueue::size() const {
    return queue_.size();
}

bool MatchQueue::compatible(const MatchCandidate &first,
                            const MatchCandidate &second,
                            std::chrono::steady_clock::time_point now) const {
    if (first.party_size < rule_.min_party_size ||
        first.party_size > rule_.max_party_size ||
        second.party_size < rule_.min_party_size ||
        second.party_size > rule_.max_party_size) {
        return false;
    }

    auto wait_first = std::chrono::duration<double>(now - first.enqueue_time).count();
    auto wait_second = std::chrono::duration<double>(now - second.enqueue_time).count();
    double wait_seconds = std::min(wait_first, wait_second);
    int allowed_delta = rule_.max_mmr_delta +
                        static_cast<int>(rule_.expansion_per_second * wait_seconds);
    int delta = std::abs(first.mmr - second.mmr);
    return delta <= allowed_delta;
}

}  // namespace match
