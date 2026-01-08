#include "net/protocol.h"

#include <algorithm>
#include <limits>

namespace net {

namespace {

bool read_u16(const std::vector<std::uint8_t> &payload, std::size_t &offset,
              std::uint16_t &out) {
    if (offset + 2 > payload.size()) {
        return false;
    }
    out = static_cast<std::uint16_t>((payload[offset] << 8) | payload[offset + 1]);
    offset += 2;
    return true;
}

void write_u16(std::uint16_t value, std::vector<std::uint8_t> &out) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

bool read_string(const std::vector<std::uint8_t> &payload, std::size_t &offset,
                 std::string &out) {
    std::uint16_t size = 0;
    if (!read_u16(payload, offset, size)) {
        return false;
    }
    if (offset + size > payload.size()) {
        return false;
    }
    out.assign(reinterpret_cast<const char *>(payload.data() + offset), size);
    offset += size;
    return true;
}

void write_string(const std::string &value, std::vector<std::uint8_t> &out) {
    auto length = static_cast<std::uint16_t>(
        std::min<std::size_t>(value.size(), std::numeric_limits<std::uint16_t>::max()));
    write_u16(length, out);
    out.insert(out.end(), value.begin(), value.begin() + length);
}

}  // namespace

std::vector<std::uint8_t> encodeLoginRequest(const LoginRequest &request) {
    std::vector<std::uint8_t> out;
    write_string(request.user_id, out);
    write_string(request.password, out);
    return out;
}

bool decodeLoginRequest(const std::vector<std::uint8_t> &payload, LoginRequest &out) {
    std::size_t offset = 0;
    if (!read_string(payload, offset, out.user_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.password)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodeLoginResponse(const LoginResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.accepted ? 1 : 0));
    write_string(response.token, out);
    write_string(response.message, out);
    return out;
}

bool decodeLoginResponse(const std::vector<std::uint8_t> &payload, LoginResponse &out) {
    if (payload.empty()) {
        return false;
    }
    std::size_t offset = 0;
    out.accepted = payload[offset++] != 0;
    if (!read_string(payload, offset, out.token)) {
        return false;
    }
    if (!read_string(payload, offset, out.message)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodeVersionReject(const VersionReject &reject) {
    std::vector<std::uint8_t> out;
    write_u16(reject.min_version, out);
    write_u16(reject.max_version, out);
    write_u16(reject.client_version, out);
    write_string(reject.message, out);
    return out;
}

bool decodeVersionReject(const std::vector<std::uint8_t> &payload, VersionReject &out) {
    std::size_t offset = 0;
    if (!read_u16(payload, offset, out.min_version)) {
        return false;
    }
    if (!read_u16(payload, offset, out.max_version)) {
        return false;
    }
    if (!read_u16(payload, offset, out.client_version)) {
        return false;
    }
    if (!read_string(payload, offset, out.message)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodeLogoutRequest(const LogoutRequest &) {
    return {};
}

bool decodeLogoutRequest(const std::vector<std::uint8_t> &payload, LogoutRequest &) {
    return payload.empty();
}

std::vector<std::uint8_t> encodeLogoutResponse(const LogoutResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.success ? 1 : 0));
    write_string(response.message, out);
    return out;
}

bool decodeLogoutResponse(const std::vector<std::uint8_t> &payload, LogoutResponse &out) {
    if (payload.empty()) {
        return false;
    }
    std::size_t offset = 0;
    out.success = payload[offset++] != 0;
    if (!read_string(payload, offset, out.message)) {
        return false;
    }
    return offset == payload.size();
}

}  // namespace net
