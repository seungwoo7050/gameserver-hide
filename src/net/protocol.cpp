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

bool read_u64(const std::vector<std::uint8_t> &payload, std::size_t &offset,
              std::uint64_t &out) {
    if (offset + 8 > payload.size()) {
        return false;
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | payload[offset + static_cast<std::size_t>(i)];
    }
    offset += 8;
    return true;
}

void write_u64(std::uint64_t value, std::vector<std::uint8_t> &out) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
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

bool read_string_list(const std::vector<std::uint8_t> &payload,
                      std::size_t &offset,
                      std::vector<std::string> &out) {
    std::uint16_t count = 0;
    if (!read_u16(payload, offset, count)) {
        return false;
    }
    out.clear();
    out.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        std::string value;
        if (!read_string(payload, offset, value)) {
            return false;
        }
        out.push_back(std::move(value));
    }
    return true;
}

void write_string_list(const std::vector<std::string> &values,
                       std::vector<std::uint8_t> &out) {
    auto length = static_cast<std::uint16_t>(
        std::min<std::size_t>(values.size(), std::numeric_limits<std::uint16_t>::max()));
    write_u16(length, out);
    for (std::uint16_t i = 0; i < length; ++i) {
        write_string(values[i], out);
    }
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

std::vector<std::uint8_t> encodePartyCreateRequest(const PartyCreateRequest &request) {
    std::vector<std::uint8_t> out;
    write_string(request.leader_user_id, out);
    return out;
}

bool decodePartyCreateRequest(const std::vector<std::uint8_t> &payload,
                              PartyCreateRequest &out) {
    std::size_t offset = 0;
    if (!read_string(payload, offset, out.leader_user_id)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodePartyCreateResponse(const PartyCreateResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.success ? 1 : 0));
    write_u64(response.party_id, out);
    write_string(response.message, out);
    return out;
}

bool decodePartyCreateResponse(const std::vector<std::uint8_t> &payload,
                               PartyCreateResponse &out) {
    if (payload.empty()) {
        return false;
    }
    std::size_t offset = 0;
    out.success = payload[offset++] != 0;
    if (!read_u64(payload, offset, out.party_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.message)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodePartyInviteRequest(const PartyInviteRequest &request) {
    std::vector<std::uint8_t> out;
    write_u64(request.party_id, out);
    write_string(request.inviter_user_id, out);
    write_string(request.invitee_user_id, out);
    return out;
}

bool decodePartyInviteRequest(const std::vector<std::uint8_t> &payload,
                              PartyInviteRequest &out) {
    std::size_t offset = 0;
    if (!read_u64(payload, offset, out.party_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.inviter_user_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.invitee_user_id)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodePartyInviteResponse(const PartyInviteResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.success ? 1 : 0));
    write_string(response.message, out);
    return out;
}

bool decodePartyInviteResponse(const std::vector<std::uint8_t> &payload,
                               PartyInviteResponse &out) {
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

std::vector<std::uint8_t> encodePartyAcceptRequest(const PartyAcceptRequest &request) {
    std::vector<std::uint8_t> out;
    write_u64(request.party_id, out);
    write_string(request.invitee_user_id, out);
    return out;
}

bool decodePartyAcceptRequest(const std::vector<std::uint8_t> &payload,
                              PartyAcceptRequest &out) {
    std::size_t offset = 0;
    if (!read_u64(payload, offset, out.party_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.invitee_user_id)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodePartyAcceptResponse(const PartyAcceptResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.success ? 1 : 0));
    write_string(response.message, out);
    return out;
}

bool decodePartyAcceptResponse(const std::vector<std::uint8_t> &payload,
                               PartyAcceptResponse &out) {
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

std::vector<std::uint8_t> encodePartyDisbandRequest(const PartyDisbandRequest &request) {
    std::vector<std::uint8_t> out;
    write_u64(request.party_id, out);
    write_string(request.requester_user_id, out);
    return out;
}

bool decodePartyDisbandRequest(const std::vector<std::uint8_t> &payload,
                               PartyDisbandRequest &out) {
    std::size_t offset = 0;
    if (!read_u64(payload, offset, out.party_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.requester_user_id)) {
        return false;
    }
    return offset == payload.size();
}

std::vector<std::uint8_t> encodePartyDisbandResponse(const PartyDisbandResponse &response) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(response.success ? 1 : 0));
    write_string(response.message, out);
    return out;
}

bool decodePartyDisbandResponse(const std::vector<std::uint8_t> &payload,
                                PartyDisbandResponse &out) {
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

std::vector<std::uint8_t> encodePartyEvent(const PartyEvent &event) {
    std::vector<std::uint8_t> out;
    write_u16(static_cast<std::uint16_t>(event.type), out);
    write_u64(event.party_id, out);
    write_string(event.actor_user_id, out);
    write_string(event.target_user_id, out);
    write_string_list(event.member_user_ids, out);
    write_string(event.message, out);
    return out;
}

bool decodePartyEvent(const std::vector<std::uint8_t> &payload, PartyEvent &out) {
    std::size_t offset = 0;
    std::uint16_t type = 0;
    if (!read_u16(payload, offset, type)) {
        return false;
    }
    out.type = static_cast<PartyEventType>(type);
    if (!read_u64(payload, offset, out.party_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.actor_user_id)) {
        return false;
    }
    if (!read_string(payload, offset, out.target_user_id)) {
        return false;
    }
    if (!read_string_list(payload, offset, out.member_user_ids)) {
        return false;
    }
    if (!read_string(payload, offset, out.message)) {
        return false;
    }
    return offset == payload.size();
}

}  // namespace net
