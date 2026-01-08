#pragma once

#include <chrono>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>

namespace net {

class TokenService {
public:
    explicit TokenService(std::chrono::seconds ttl = std::chrono::seconds{300});

    std::string issueToken(const std::string &user_id,
                           std::chrono::steady_clock::time_point now);
    bool validateToken(const std::string &token,
                       std::chrono::steady_clock::time_point now,
                       std::string &user_id);

private:
    struct TokenRecord {
        std::string user_id;
        std::chrono::steady_clock::time_point expires_at;
    };

    std::string generateToken();

    std::mt19937 rng_;
    std::uniform_int_distribution<std::uint8_t> dist_;
    std::unordered_map<std::string, TokenRecord> tokens_;
    std::chrono::seconds ttl_;
};

}  // namespace net
