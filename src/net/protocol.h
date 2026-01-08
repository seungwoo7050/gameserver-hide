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
    PartyEvent = 108,
    MatchReq = 400,
    MatchFoundNotify = 401,
    GuildCreateReq = 200,
    GuildCreateRes = 201,
    GuildJoinReq = 202,
    GuildJoinRes = 203,
    GuildLeaveReq = 204,
    GuildLeaveRes = 205,
    GuildEvent = 206,
    DungeonEnterReq = 500,
    DungeonEnterRes = 501,
    DungeonResultNotify = 502,
    DungeonResultRes = 503,
    ChatSendReq = 300,
    ChatSendRes = 301,
    ChatEvent = 302,
    InventoryUpdateNotify = 600,
    InventoryUpdateRes = 601
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

enum class GuildEventType : std::uint16_t {
    Created = 1,
    Joined = 2,
    Left = 3,
    Disbanded = 4
};

struct GuildCreateRequest {
    std::string guild_name;
};

struct GuildCreateResponse {
    bool success{false};
    std::uint64_t guild_id{0};
    std::string message;
};

struct GuildJoinRequest {
    std::uint64_t guild_id{0};
};

struct GuildJoinResponse {
    bool success{false};
    std::string message;
};

struct GuildLeaveRequest {
    std::uint64_t guild_id{0};
};

struct GuildLeaveResponse {
    bool success{false};
    std::string message;
};

struct GuildEvent {
    GuildEventType type{GuildEventType::Created};
    std::uint64_t guild_id{0};
    std::string actor_user_id;
    std::vector<std::string> member_user_ids;
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

struct MatchRequest {
    std::uint64_t party_id{0};
    std::uint32_t dungeon_id{0};
    std::string difficulty;
};

struct MatchFoundNotify {
    bool success{false};
    std::string code;
    std::string message;
    std::uint64_t party_id{0};
    std::uint64_t instance_id{0};
    std::string endpoint;
    std::string ticket;
};

struct DungeonEnterRequest {
    std::uint64_t instance_id{0};
    std::string ticket;
    std::uint64_t char_id{0};
};

enum class DungeonState : std::uint16_t {
    Waiting = 0,
    Ready = 1,
    Playing = 2,
    Clear = 3,
    Fail = 4,
    Terminate = 5
};

struct DungeonEnterResponse {
    bool success{false};
    std::string code;
    std::string message;
    DungeonState state{DungeonState::Waiting};
    std::uint32_t seed{0};
};

enum class DungeonResultType : std::uint16_t {
    Clear = 1,
    Fail = 2
};

struct RewardItem {
    std::uint32_t item_id{0};
    std::uint32_t count{0};
};

struct DungeonResultNotify {
    DungeonResultType result{DungeonResultType::Clear};
    std::uint32_t time_sec{0};
    std::uint16_t deaths{0};
    std::vector<RewardItem> rewards;
};

struct DungeonResultResponse {
    bool success{false};
    std::string code;
    std::string message;
    std::string summary;
};

struct InventoryUpdateNotify {
    std::uint64_t char_id{0};
    std::vector<RewardItem> items;
};

struct InventoryUpdateResponse {
    bool success{false};
    std::string code;
    std::string message;
    std::uint64_t inventory_version{0};
};

enum class ChatChannel : std::uint16_t {
    Global = 1,
    Party = 2
};

struct ChatSendRequest {
    ChatChannel channel{ChatChannel::Global};
    std::uint64_t party_id{0};
    std::string message;
};

struct ChatSendResponse {
    bool success{false};
    std::string message;
};

struct ChatEvent {
    ChatChannel channel{ChatChannel::Global};
    std::uint64_t party_id{0};
    std::string sender_user_id;
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

std::vector<std::uint8_t> encodeGuildCreateRequest(const GuildCreateRequest &request);
bool decodeGuildCreateRequest(const std::vector<std::uint8_t> &payload,
                              GuildCreateRequest &out);

std::vector<std::uint8_t> encodeGuildCreateResponse(const GuildCreateResponse &response);
bool decodeGuildCreateResponse(const std::vector<std::uint8_t> &payload,
                               GuildCreateResponse &out);

std::vector<std::uint8_t> encodeGuildJoinRequest(const GuildJoinRequest &request);
bool decodeGuildJoinRequest(const std::vector<std::uint8_t> &payload,
                            GuildJoinRequest &out);

std::vector<std::uint8_t> encodeGuildJoinResponse(const GuildJoinResponse &response);
bool decodeGuildJoinResponse(const std::vector<std::uint8_t> &payload,
                             GuildJoinResponse &out);

std::vector<std::uint8_t> encodeGuildLeaveRequest(const GuildLeaveRequest &request);
bool decodeGuildLeaveRequest(const std::vector<std::uint8_t> &payload,
                             GuildLeaveRequest &out);

std::vector<std::uint8_t> encodeGuildLeaveResponse(const GuildLeaveResponse &response);
bool decodeGuildLeaveResponse(const std::vector<std::uint8_t> &payload,
                              GuildLeaveResponse &out);

std::vector<std::uint8_t> encodeGuildEvent(const GuildEvent &event);
bool decodeGuildEvent(const std::vector<std::uint8_t> &payload, GuildEvent &out);

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

std::vector<std::uint8_t> encodeMatchRequest(const MatchRequest &request);
bool decodeMatchRequest(const std::vector<std::uint8_t> &payload, MatchRequest &out);

std::vector<std::uint8_t> encodeMatchFoundNotify(const MatchFoundNotify &notify);
bool decodeMatchFoundNotify(const std::vector<std::uint8_t> &payload,
                            MatchFoundNotify &out);

std::vector<std::uint8_t> encodeDungeonEnterRequest(const DungeonEnterRequest &request);
bool decodeDungeonEnterRequest(const std::vector<std::uint8_t> &payload,
                               DungeonEnterRequest &out);

std::vector<std::uint8_t> encodeDungeonEnterResponse(const DungeonEnterResponse &response);
bool decodeDungeonEnterResponse(const std::vector<std::uint8_t> &payload,
                                DungeonEnterResponse &out);

std::vector<std::uint8_t> encodeDungeonResultNotify(const DungeonResultNotify &notify);
bool decodeDungeonResultNotify(const std::vector<std::uint8_t> &payload,
                               DungeonResultNotify &out);

std::vector<std::uint8_t> encodeDungeonResultResponse(const DungeonResultResponse &response);
bool decodeDungeonResultResponse(const std::vector<std::uint8_t> &payload,
                                 DungeonResultResponse &out);

std::vector<std::uint8_t> encodeChatSendRequest(const ChatSendRequest &request);
bool decodeChatSendRequest(const std::vector<std::uint8_t> &payload,
                           ChatSendRequest &out);

std::vector<std::uint8_t> encodeChatSendResponse(const ChatSendResponse &response);
bool decodeChatSendResponse(const std::vector<std::uint8_t> &payload,
                            ChatSendResponse &out);

std::vector<std::uint8_t> encodeChatEvent(const ChatEvent &event);
bool decodeChatEvent(const std::vector<std::uint8_t> &payload, ChatEvent &out);

std::vector<std::uint8_t> encodeInventoryUpdateNotify(const InventoryUpdateNotify &notify);
bool decodeInventoryUpdateNotify(const std::vector<std::uint8_t> &payload,
                                 InventoryUpdateNotify &out);

std::vector<std::uint8_t> encodeInventoryUpdateResponse(
    const InventoryUpdateResponse &response);
bool decodeInventoryUpdateResponse(const std::vector<std::uint8_t> &payload,
                                   InventoryUpdateResponse &out);

}  // namespace net
