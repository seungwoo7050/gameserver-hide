#include "admin/admin.h"
#include "admin/logging.h"
#include "net/auth.h"
#include "net/codec.h"
#include "net/protocol.h"
#include "net/server.h"
#include "net/session.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void write_u32(std::uint32_t value, std::vector<std::uint8_t> &out) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void write_u16(std::uint16_t value, std::vector<std::uint8_t> &out) {
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

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

class CoutCapture {
public:
    CoutCapture() : old_(std::cout.rdbuf(buffer_.rdbuf())) {}
    ~CoutCapture() {
        std::cout.rdbuf(old_);
    }

    std::string str() const {
        return buffer_.str();
    }

private:
    std::ostringstream buffer_;
    std::streambuf *old_;
};

bool is_hex_string(const std::string &value) {
    if (value.size() != 32) {
        return false;
    }
    for (char ch : value) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    using namespace std::chrono;

    {
        net::FrameDecoder decoder;
        net::FrameHeader header{};
        std::vector<std::uint8_t> payload;
        std::vector<std::uint8_t> frame;
        frame.push_back(0x00);
        frame.push_back(0x00);
        frame.push_back(0x00);
        decoder.append(frame);
        assert(!decoder.nextFrame(header, payload));
    }

    {
        std::vector<std::uint8_t> frame;
        write_u32(10, frame);
        write_u16(1, frame);
        write_u16(1, frame);
        frame.push_back(0xAA);
        frame.push_back(0xBB);
        net::FrameDecoder decoder;
        decoder.append(frame);
        net::FrameHeader header{};
        std::vector<std::uint8_t> payload;
        assert(!decoder.nextFrame(header, payload));
    }

    {
        std::vector<std::uint8_t> payload = {0x10, 0x20};
        auto frame = net::Codec::encode(7, 2, payload);
        net::FrameDecoder decoder;
        decoder.append(frame);
        net::FrameHeader header{};
        std::vector<std::uint8_t> decoded;
        assert(decoder.nextFrame(header, decoded));
        assert(header.length == payload.size());
        assert(header.type == 7);
        assert(header.version == 2);
        assert(decoded == payload);
    }

    {
        net::SessionConfig config;
        config.heartbeat_interval = milliseconds{1000};
        config.timeout = milliseconds{2000};
        auto start = steady_clock::now();
        net::Session session(1, config, start);
        assert(!session.shouldSendHeartbeat(start));
        auto later = start + milliseconds{1500};
        assert(session.shouldSendHeartbeat(later));
        session.markHeartbeatSent(later);
        auto timeout_time = start + milliseconds{2500};
        assert(!session.tick(timeout_time));
        assert(!session.connected());
    }

    {
        net::SessionConfig config;
        config.send_queue_limit_bytes = 6;
        config.overflow_policy = net::OverflowPolicy::DropOldest;
        auto now = steady_clock::now();
        net::Session session(2, config, now);
        assert(session.enqueueSend(std::vector<std::uint8_t>(4, 0xAA), now));
        assert(session.enqueueSend(std::vector<std::uint8_t>(4, 0xBB), now));
        assert(session.queuedBytes() <= config.send_queue_limit_bytes);
    }

    {
        net::SessionConfig config;
        config.send_queue_limit_bytes = 4;
        config.overflow_policy = net::OverflowPolicy::DropNewest;
        auto now = steady_clock::now();
        net::Session session(3, config, now);
        assert(!session.enqueueSend(std::vector<std::uint8_t>(8, 0xCC), now));
        assert(session.connected());
    }

    {
        net::SessionConfig config;
        config.send_queue_limit_bytes = 4;
        config.overflow_policy = net::OverflowPolicy::Disconnect;
        auto now = steady_clock::now();
        net::Session session(4, config, now);
        assert(!session.enqueueSend(std::vector<std::uint8_t>(8, 0xDD), now));
        assert(!session.connected());
    }

    {
        net::SessionConfig config;
        config.rate_limit_capacity = 4.0;
        config.rate_limit_refill_per_sec = 4.0;
        auto now = steady_clock::now();
        net::Session session(5, config, now);
        assert(session.enqueueSend(std::vector<std::uint8_t>(4, 0x11), now));
        assert(!session.enqueueSend(std::vector<std::uint8_t>(1, 0x12), now));
        auto later = now + seconds{1};
        assert(session.enqueueSend(std::vector<std::uint8_t>(1, 0x13), later));
    }

    {
        net::TokenService tokens(seconds{1});
        auto now = steady_clock::now();
        auto token = tokens.issueToken("user1", now);
        std::string user_id;
        assert(tokens.validateToken(token, now, user_id));
        assert(user_id == "user1");
        auto later = now + seconds{2};
        assert(!tokens.validateToken(token, later, user_id));
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto login_payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(login_payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response_frame = server.handlePacket(*session, header, login_payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::LoginRes,
                            net::kMinProtocolVersion, response_payload);
        net::LoginResponse response;
        assert(net::decodeLoginResponse(response_payload, response));
        assert(response.accepted);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session1 = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto login_payload = net::encodeLoginRequest(login);
        net::FrameHeader login_header{static_cast<std::uint32_t>(login_payload.size()),
                                      static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                      net::kMinProtocolVersion};
        auto login_response =
            server.handlePacket(*session1, login_header, login_payload, now);
        assert(login_response.has_value());
        std::vector<std::uint8_t> login_payload_out;
        assert_payload_type(*login_response, net::PacketType::LoginRes,
                            net::kMinProtocolVersion, login_payload_out);
        net::LoginResponse login_result;
        assert(net::decodeLoginResponse(login_payload_out, login_result));
        assert(login_result.accepted);

        session1->setLastSeq(5);

        auto session2 = server.createSession(config, now);
        net::SessionReconnectRequest reconnect{login_result.token, 7};
        auto reconnect_payload = net::encodeSessionReconnectRequest(reconnect);
        net::FrameHeader reconnect_header{
            static_cast<std::uint32_t>(reconnect_payload.size()),
            static_cast<std::uint16_t>(net::PacketType::SessionReconnectReq),
            net::kMinProtocolVersion};
        auto reconnect_response =
            server.handlePacket(*session2, reconnect_header, reconnect_payload, now);
        assert(reconnect_response.has_value());
        std::vector<std::uint8_t> reconnect_payload_out;
        assert_payload_type(*reconnect_response, net::PacketType::SessionReconnectRes,
                            net::kMinProtocolVersion, reconnect_payload_out);
        net::SessionReconnectResponse reconnect_result;
        assert(net::decodeSessionReconnectResponse(reconnect_payload_out,
                                                   reconnect_result));
        assert(reconnect_result.success);
        assert(reconnect_result.session_id == session2->id());
        assert(reconnect_result.resume_from_seq == 8);
        assert(server.sessionUser(session2->id()) != nullptr);
        assert(server.sessionUser(session1->id()) == nullptr);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::SessionReconnectRequest reconnect{"invalid-token", 0};
        auto reconnect_payload = net::encodeSessionReconnectRequest(reconnect);
        net::FrameHeader reconnect_header{
            static_cast<std::uint32_t>(reconnect_payload.size()),
            static_cast<std::uint16_t>(net::PacketType::SessionReconnectReq),
            net::kMinProtocolVersion};
        auto reconnect_response =
            server.handlePacket(*session, reconnect_header, reconnect_payload, now);
        assert(reconnect_response.has_value());
        std::vector<std::uint8_t> reconnect_payload_out;
        assert_payload_type(*reconnect_response, net::PacketType::SessionReconnectRes,
                            net::kMinProtocolVersion, reconnect_payload_out);
        net::SessionReconnectResponse reconnect_result;
        assert(net::decodeSessionReconnectResponse(reconnect_payload_out,
                                                   reconnect_result));
        assert(!reconnect_result.success);
        assert(reconnect_result.message == "Invalid or expired token");
        assert(server.sessionUser(session->id()) == nullptr);
        assert(session->lastSeq() == 0);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto login_payload = net::encodeLoginRequest(login);
        net::FrameHeader login_header{static_cast<std::uint32_t>(login_payload.size()),
                                      static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                      net::kMinProtocolVersion};
        auto login_response =
            server.handlePacket(*session, login_header, login_payload, now);
        assert(login_response.has_value());
        std::vector<std::uint8_t> login_payload_out;
        assert_payload_type(*login_response, net::PacketType::LoginRes,
                            net::kMinProtocolVersion, login_payload_out);
        net::LoginResponse login_result;
        assert(net::decodeLoginResponse(login_payload_out, login_result));
        assert(login_result.accepted);

        net::SessionReconnectRequest reconnect{login_result.token, 0};
        auto reconnect_payload = net::encodeSessionReconnectRequest(reconnect);
        net::FrameHeader reconnect_header{
            static_cast<std::uint32_t>(reconnect_payload.size()),
            static_cast<std::uint16_t>(net::PacketType::SessionReconnectReq),
            net::kMinProtocolVersion};
        auto reconnect_response = server.handlePacket(*session,
                                                      reconnect_header,
                                                      reconnect_payload,
                                                      now + seconds{400});
        assert(reconnect_response.has_value());
        std::vector<std::uint8_t> reconnect_payload_out;
        assert_payload_type(*reconnect_response, net::PacketType::SessionReconnectRes,
                            net::kMinProtocolVersion, reconnect_payload_out);
        net::SessionReconnectResponse reconnect_result;
        assert(net::decodeSessionReconnectResponse(reconnect_payload_out,
                                                   reconnect_result));
        assert(!reconnect_result.success);
        assert(reconnect_result.message == "Invalid or expired token");
        const auto *user = server.sessionUser(session->id());
        assert(user != nullptr);
        assert(user->user_id == "user1");
        assert(session->lastSeq() == 0);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "badpw"};
        auto login_payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(login_payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response_frame = server.handlePacket(*session, header, login_payload, now);
        assert(response_frame.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response_frame, net::PacketType::LoginRes,
                            net::kMinProtocolVersion, response_payload);
        net::LoginResponse response;
        assert(net::decodeLoginResponse(response_payload, response));
        assert(!response.accepted);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session1 = server.createSession(config, now);
        auto session2 = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response1 = server.handlePacket(*session1, header, payload, now);
        assert(response1.has_value());
        auto response2 = server.handlePacket(*session2, header, payload, now);
        assert(response2.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response2, net::PacketType::LoginRes,
                            net::kMinProtocolVersion, response_payload);
        net::LoginResponse response;
        assert(net::decodeLoginResponse(response_payload, response));
        assert(!response.accepted);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response = server.handlePacket(*session, header, payload, now);
        assert(response.has_value());
        net::LogoutRequest logout;
        auto logout_payload = net::encodeLogoutRequest(logout);
        net::FrameHeader logout_header{0,
                                       static_cast<std::uint16_t>(net::PacketType::LogoutReq),
                                       net::kMinProtocolVersion};
        auto logout_response =
            server.handlePacket(*session, logout_header, logout_payload, now);
        assert(logout_response.has_value());
        assert(server.sessionUser(session->id()) == nullptr);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response = server.handlePacket(*session, header, payload, now);
        assert(response.has_value());
        server.removeSession(session->id());
        assert(server.sessionUser(session->id()) == nullptr);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "letmein"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                static_cast<std::uint16_t>(net::kMaxProtocolVersion + 1)};
        auto response = server.handlePacket(*session, header, payload, now);
        assert(response.has_value());
        std::vector<std::uint8_t> response_payload;
        assert_payload_type(*response, net::PacketType::VersionReject,
                            static_cast<std::uint16_t>(net::kMaxProtocolVersion + 1),
                            response_payload);
        net::VersionReject reject;
        assert(net::decodeVersionReject(response_payload, reject));
        assert(reject.client_version == net::kMaxProtocolVersion + 1);
    }

    {
        admin::StructuredLogger logger;
        admin::LogFields fields;
        fields.request_trace_id = "req-1";
        fields.session_trace_id = "sess-1";
        fields.session_id = 42;
        fields.reason = "testing";
        CoutCapture capture;
        logger.log("info", "test_event", "Testing logging", fields);
        const auto output = capture.str();
        assert(output.find("\"level\":\"info\"") != std::string::npos);
        assert(output.find("\"event\":\"test_event\"") != std::string::npos);
        assert(output.find("\"message\":\"Testing logging\"") != std::string::npos);
        assert(output.find("\"request_trace_id\":\"req-1\"") != std::string::npos);
        assert(output.find("\"session_trace_id\":\"sess-1\"") != std::string::npos);
        assert(output.find("\"session_id\":42") != std::string::npos);
        assert(output.find("\"reason\":\"testing\"") != std::string::npos);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        assert(is_hex_string(session->traceId()));
        net::LoginRequest login{"user1", "letmein"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        CoutCapture capture;
        auto response = server.handlePacket(*session, header, payload, now);
        assert(response.has_value());
        const auto logs = capture.str();
        assert(logs.find("\"event\":\"packet_received\"") != std::string::npos);
        assert(logs.find("\"request_trace_id\":\"") != std::string::npos);
        assert(logs.find("\"session_trace_id\":\"" + session->traceId() + "\"") !=
               std::string::npos);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        net::LoginRequest login{"user1", "badpw"};
        auto payload = net::encodeLoginRequest(login);
        net::FrameHeader header{static_cast<std::uint32_t>(payload.size()),
                                static_cast<std::uint16_t>(net::PacketType::LoginReq),
                                net::kMinProtocolVersion};
        auto response = server.handlePacket(*session, header, payload, now);
        assert(response.has_value());
        auto metrics = server.metrics();
        assert(metrics.packets_total == 1);
        assert(metrics.bytes_total == payload.size());
        assert(metrics.error_total == 1);
        assert(server.sessionCount() == 1);
    }

    {
        net::Server server;
        net::SessionConfig config;
        auto now = steady_clock::now();
        auto session = server.createSession(config, now);
        admin::AdminService admin_service(server);
        auto status = admin_service.getStatus();
        assert(status.active_sessions == 1);
        assert(status.packets_total == 0);
        assert(status.error_total == 0);
        assert(admin_service.forceTerminateSession(session->id(), "maintenance"));
        assert(server.sessionCount() == 0);
        assert(!admin_service.forceTerminateSession(999, "missing"));
    }

    std::cout << "All tests passed.\n";
    return 0;
}
