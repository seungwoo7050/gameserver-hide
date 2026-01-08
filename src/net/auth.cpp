#include "net/auth.h"

#include <array>
#include <sstream>

namespace net {

TokenService::TokenService(std::chrono::seconds ttl)
    : rng_(std::random_device{}()), dist_(0, 255), ttl_(ttl) {}

std::string TokenService::issueToken(const std::string &user_id,
                                     std::chrono::steady_clock::time_point now) {
    auto token = generateToken();
    tokens_[token] = TokenRecord{user_id, now + ttl_};
    return token;
}

bool TokenService::validateToken(const std::string &token,
                                 std::chrono::steady_clock::time_point now,
                                 std::string &user_id) {
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        return false;
    }
    if (now >= it->second.expires_at) {
        tokens_.erase(it);
        return false;
    }
    user_id = it->second.user_id;
    return true;
}

std::string TokenService::generateToken() {
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        auto value = dist_(rng_);
        oss << std::hex << std::uppercase << (value >> 4);
        oss << std::hex << std::uppercase << (value & 0xF);
    }
    return oss.str();
}

}  // namespace net
