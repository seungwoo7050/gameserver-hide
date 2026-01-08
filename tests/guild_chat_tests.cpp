#include "net/codec.h"
#include "net/protocol.h"
#include "net/server.h"
#include "net/session.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace {

net::FrameHeader decode_header(const std::vector<std::uint8_t> &frame,
                               std::vector<std::uint8_t> &payload) {
    net::FrameDecoder decoder;
    decoder.append(frame);
    net::FrameHeader header{};
    bool ok = decoder.nextFrame(header, payload);
    assert(ok);
    return header;
}

void assert_payload_type(const std::vector<std::uint8_t> &frame,
                         net::PacketType expected_type,
                         std::uint16_t expected_version,
                         std::vector<std::uint8_t> &payload) {
    auto header = decode_header(frame, payload);
    assert(header.type == static_cast<std::uint16_t>(expected_type));
    assert(header.version == expected_version);
}

net::LoginResponse login(net::Server &server,
                         net::Session &session,
                         const std::string &user_id,
                         std::uint16_t version) {
    net::LoginRequest request{user_id, "letmein"};
    auto payload = net::encodeLoginRequest(request);
    net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                            static_cast<std::uint16_t>(net::PacketType::LoginReq),
                            version};
    auto response_frame = server.handlePacket(session, header, payload,
                                              std::chrono::steady_clock::now());
    assert(response_frame.has_value());
    std::vector<std::uint8_t> response_payload;
    assert_payload_type(*response_frame, net::PacketType::LoginRes, version,
                        response_payload);
    net::LoginResponse response;
    assert(net::decodeLoginResponse(response_payload, response));
    assert(response.accepted);
    return response;
}

bool dequeue_frame(net::Session &session,
                   net::PacketType expected_type,
                   std::uint16_t expected_version,
                   std::vector<std::uint8_t> &payload) {
    std::vector<std::uint8_t> frame;
    if (!session.dequeueSend(frame)) {
        return false;
    }
    assert_payload_type(frame, expected_type, expected_version, payload);
    return true;
}

}  // namespace

int main() {
    using namespace std::chrono;

    net::Server server;
    net::SessionConfig config;
    auto now = steady_clock::now();
    auto session1 = server.createSession(config, now);
    auto session2 = server.createSession(config, now);
    auto session3 = server.createSession(config, now);
    auto version = net::kMinProtocolVersion;

    login(server, *session1, "leader", version);
    login(server, *session2, "member", version);
    login(server, *session3, "outsider", version);

    std::uint64_t guild_id = 0;
    {
        net::GuildCreateRequest request{"Heroes"};
        auto payload = net::encodeGuildCreateRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::GuildCreateReq),
                                version};
        auto response_frame = server.handlePacket(*session1, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::GuildCreateRes,
                            version, response_payload);
        net::GuildCreateResponse response;
        assert(net::decodeGuildCreateResponse(response_payload, response));
        assert(response.success);
        guild_id = response.guild_id;
        assert(guild_id != 0);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::GuildEvent, version,
                             event_payload));
        net::GuildEvent event;
        assert(net::decodeGuildEvent(event_payload, event));
        assert(event.type == net::GuildEventType::Created);
        assert(event.guild_id == guild_id);
    }

    {
        net::GuildJoinRequest request{guild_id};
        auto payload = net::encodeGuildJoinRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::GuildJoinReq),
                                version};
        auto response_frame = server.handlePacket(*session2, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::GuildJoinRes,
                            version, response_payload);
        net::GuildJoinResponse response;
        assert(net::decodeGuildJoinResponse(response_payload, response));
        assert(response.success);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::GuildEvent, version,
                             event_payload));
        net::GuildEvent event;
        assert(net::decodeGuildEvent(event_payload, event));
        assert(event.type == net::GuildEventType::Joined);
        assert(event.guild_id == guild_id);

        assert(dequeue_frame(*session2, net::PacketType::GuildEvent, version,
                             event_payload));
    }

    {
        net::GuildJoinRequest request{guild_id};
        auto payload = net::encodeGuildJoinRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::GuildJoinReq),
                                version};
        auto response_frame = server.handlePacket(*session2, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::GuildJoinRes,
                            version, response_payload);
        net::GuildJoinResponse response;
        assert(net::decodeGuildJoinResponse(response_payload, response));
        assert(!response.success);
    }

    {
        net::GuildLeaveRequest request{guild_id};
        auto payload = net::encodeGuildLeaveRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::GuildLeaveReq),
                                version};
        auto response_frame = server.handlePacket(*session2, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::GuildLeaveRes,
                            version, response_payload);
        net::GuildLeaveResponse response;
        assert(net::decodeGuildLeaveResponse(response_payload, response));
        assert(response.success);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::GuildEvent, version,
                             event_payload));
        net::GuildEvent event;
        assert(net::decodeGuildEvent(event_payload, event));
        assert(event.type == net::GuildEventType::Left);

        assert(dequeue_frame(*session2, net::PacketType::GuildEvent, version,
                             event_payload));
    }

    {
        net::GuildLeaveRequest request{guild_id};
        auto payload = net::encodeGuildLeaveRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::GuildLeaveReq),
                                version};
        auto response_frame = server.handlePacket(*session3, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::GuildLeaveRes,
                            version, response_payload);
        net::GuildLeaveResponse response;
        assert(net::decodeGuildLeaveResponse(response_payload, response));
        assert(!response.success);
    }

    {
        net::ChatSendRequest request{net::ChatChannel::Global, 0, "Hello world"};
        auto payload = net::encodeChatSendRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::ChatSendReq),
                                version};
        auto response_frame = server.handlePacket(*session1, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::ChatSendRes,
                            version, response_payload);
        net::ChatSendResponse response;
        assert(net::decodeChatSendResponse(response_payload, response));
        assert(response.success);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::ChatEvent, version,
                             event_payload));
        net::ChatEvent event;
        assert(net::decodeChatEvent(event_payload, event));
        assert(event.channel == net::ChatChannel::Global);
        assert(event.sender_user_id == "leader");
        assert(event.message == "Hello world");

        assert(dequeue_frame(*session2, net::PacketType::ChatEvent, version,
                             event_payload));
    }

    {
        auto party_id = server.partyService().createParty(session1->id(), "leader");
        assert(party_id.has_value());
        assert(server.partyService().inviteMember(*party_id,
                                                  session1->id(),
                                                  session2->id(),
                                                  "member",
                                                  now));
        assert(server.partyService().acceptInvite(*party_id, session2->id(), now));

        net::ChatSendRequest request{net::ChatChannel::Party, 0, "Party time"};
        auto payload = net::encodeChatSendRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::ChatSendReq),
                                version};
        auto response_frame = server.handlePacket(*session1, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::ChatSendRes,
                            version, response_payload);
        net::ChatSendResponse response;
        assert(net::decodeChatSendResponse(response_payload, response));
        assert(response.success);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::ChatEvent, version,
                             event_payload));
        net::ChatEvent event;
        assert(net::decodeChatEvent(event_payload, event));
        assert(event.channel == net::ChatChannel::Party);
        assert(event.message == "Party time");

        assert(dequeue_frame(*session2, net::PacketType::ChatEvent, version,
                             event_payload));
    }

    {
        server.removeSession(session2->id());
        net::ChatSendRequest request{net::ChatChannel::Party, 0, "Still here"};
        auto payload = net::encodeChatSendRequest(request);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::ChatSendReq),
                                version};
        auto response_frame = server.handlePacket(*session1, header, payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::ChatSendRes,
                            version, response_payload);
        net::ChatSendResponse response;
        assert(net::decodeChatSendResponse(response_payload, response));
        assert(response.success);

        std::vector<std::uint8_t> event_payload;
        assert(dequeue_frame(*session1, net::PacketType::ChatEvent, version,
                             event_payload));

        auto session2b = server.createSession(config, now);
        login(server, *session2b, "member", version);
        net::ChatSendRequest party_request{net::ChatChannel::Party, 0, "Hello"};
        auto party_payload = net::encodeChatSendRequest(party_request);
        net::FrameHeader party_header{static_cast<std::uint32_t>(party_payload.size()),
                                      static_cast<std::uint16_t>(net::PacketType::ChatSendReq),
                                      version};
        auto party_response = server.handlePacket(*session2b, party_header,
                                                  party_payload, now);
        assert(party_response.has_value());
        std::vector<std::uint8_t> party_response_payload;
        assert_payload_type(*party_response, net::PacketType::ChatSendRes,
                            version, party_response_payload);
        net::ChatSendResponse party_result;
        assert(net::decodeChatSendResponse(party_response_payload, party_result));
        assert(!party_result.success);
    }

    return 0;
}
