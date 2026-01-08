#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace net {

constexpr std::uint16_t kMinProtocolVersion = 1;
constexpr std::uint16_t kMaxProtocolVersion = 3;

enum class PacketType : std::uint16_t {
    LoginReq = 1,
    LoginRes = 2,
    VersionReject = 3,
    LogoutReq = 4,
    LogoutRes = 5
};

struct LoginRequest {
    std::string user_id;
    std::string password;
};

struct LoginResponse {
    bool accepted{false};
    std::string token;
    std::string message;
};

struct VersionReject {
    std::uint16_t min_version{0};
    std::uint16_t max_version{0};
    std::uint16_t client_version{0};
    std::string message;
};

struct LogoutRequest {};

struct LogoutResponse {
    bool success{false};
    std::string message;
};

std::vector<std::uint8_t> encodeLoginRequest(const LoginRequest &request);
bool decodeLoginRequest(const std::vector<std::uint8_t> &payload, LoginRequest &out);

std::vector<std::uint8_t> encodeLoginResponse(const LoginResponse &response);
bool decodeLoginResponse(const std::vector<std::uint8_t> &payload, LoginResponse &out);

std::vector<std::uint8_t> encodeVersionReject(const VersionReject &reject);
bool decodeVersionReject(const std::vector<std::uint8_t> &payload, VersionReject &out);

std::vector<std::uint8_t> encodeLogoutRequest(const LogoutRequest &request);
bool decodeLogoutRequest(const std::vector<std::uint8_t> &payload, LogoutRequest &out);

std::vector<std::uint8_t> encodeLogoutResponse(const LogoutResponse &response);
bool decodeLogoutResponse(const std::vector<std::uint8_t> &payload, LogoutResponse &out);

}  // namespace net
