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
    LogoutRes = 5,
    PartyCreateReq = 100,
    PartyCreateRes = 101,
    PartyInviteReq = 102,
    PartyInviteRes = 103,
    PartyAcceptReq = 104,
    PartyAcceptRes = 105,
    PartyDisbandReq = 106,
    PartyDisbandRes = 107,
    PartyEvent = 108
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

enum class PartyEventType : std::uint16_t {
    Created = 1,
    InviteSent = 2,
    InviteAccepted = 3,
    InviteRejected = 4,
    InviteExpired = 5,
    Disbanded = 6
};

struct PartyCreateRequest {
    std::string leader_user_id;
};

struct PartyCreateResponse {
    bool success{false};
    std::uint64_t party_id{0};
    std::string message;
};

struct PartyInviteRequest {
    std::uint64_t party_id{0};
    std::string inviter_user_id;
    std::string invitee_user_id;
};

struct PartyInviteResponse {
    bool success{false};
    std::string message;
};

struct PartyAcceptRequest {
    std::uint64_t party_id{0};
    std::string invitee_user_id;
};

struct PartyAcceptResponse {
    bool success{false};
    std::string message;
};

struct PartyDisbandRequest {
    std::uint64_t party_id{0};
    std::string requester_user_id;
};

struct PartyDisbandResponse {
    bool success{false};
    std::string message;
};

struct PartyEvent {
    PartyEventType type{PartyEventType::Created};
    std::uint64_t party_id{0};
    std::string actor_user_id;
    std::string target_user_id;
    std::vector<std::string> member_user_ids;
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

std::vector<std::uint8_t> encodePartyCreateRequest(const PartyCreateRequest &request);
bool decodePartyCreateRequest(const std::vector<std::uint8_t> &payload,
                              PartyCreateRequest &out);

std::vector<std::uint8_t> encodePartyCreateResponse(const PartyCreateResponse &response);
bool decodePartyCreateResponse(const std::vector<std::uint8_t> &payload,
                               PartyCreateResponse &out);

std::vector<std::uint8_t> encodePartyInviteRequest(const PartyInviteRequest &request);
bool decodePartyInviteRequest(const std::vector<std::uint8_t> &payload,
                              PartyInviteRequest &out);

std::vector<std::uint8_t> encodePartyInviteResponse(const PartyInviteResponse &response);
bool decodePartyInviteResponse(const std::vector<std::uint8_t> &payload,
                               PartyInviteResponse &out);

std::vector<std::uint8_t> encodePartyAcceptRequest(const PartyAcceptRequest &request);
bool decodePartyAcceptRequest(const std::vector<std::uint8_t> &payload,
                              PartyAcceptRequest &out);

std::vector<std::uint8_t> encodePartyAcceptResponse(const PartyAcceptResponse &response);
bool decodePartyAcceptResponse(const std::vector<std::uint8_t> &payload,
                               PartyAcceptResponse &out);

std::vector<std::uint8_t> encodePartyDisbandRequest(const PartyDisbandRequest &request);
bool decodePartyDisbandRequest(const std::vector<std::uint8_t> &payload,
                               PartyDisbandRequest &out);

std::vector<std::uint8_t> encodePartyDisbandResponse(const PartyDisbandResponse &response);
bool decodePartyDisbandResponse(const std::vector<std::uint8_t> &payload,
                                PartyDisbandResponse &out);

std::vector<std::uint8_t> encodePartyEvent(const PartyEvent &event);
bool decodePartyEvent(const std::vector<std::uint8_t> &payload, PartyEvent &out);

}  // namespace net
