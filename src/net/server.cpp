#include "net/server.h"

#include <chrono>
#include <optional>
#include <sstream>
#include <vector>

namespace net {

Server::Server() : started_at_(std::chrono::steady_clock::now()) {
    logger_.log("info", "server_started", "Server started");
    guild_service_.setEventSink([this](SessionId session_id,
                                       const guild::GuildEvent &event) {
        auto session = findSession(session_id);
        if (!session) {
            return;
        }
        GuildEvent payload;
        payload.type = static_cast<GuildEventType>(event.type);
        payload.guild_id = event.guild_id;
        payload.actor_user_id = event.actor_user_id;
        payload.member_user_ids = event.member_user_ids;
        payload.message = event.message;
        auto encoded = encodeGuildEvent(payload);
        auto frame = Codec::encode(static_cast<std::uint16_t>(PacketType::GuildEvent),
                                   session->protocolVersion(),
                                   encoded);
        session->enqueueSend(std::move(frame), std::chrono::steady_clock::now());
    });

    chat_service_.setEventSink([this](SessionId session_id,
                                      const chat::ChatMessage &message) {
        auto session = findSession(session_id);
        if (!session) {
            return;
        }
        ChatEvent payload;
        payload.channel = message.channel == chat::ChatChannel::Party
                              ? ChatChannel::Party
                              : ChatChannel::Global;
        payload.party_id = message.party_id;
        payload.sender_user_id = message.sender_user_id;
        payload.message = message.text;
        auto encoded = encodeChatEvent(payload);
        auto frame = Codec::encode(static_cast<std::uint16_t>(PacketType::ChatEvent),
                                   session->protocolVersion(),
                                   encoded);
        session->enqueueSend(std::move(frame), std::chrono::steady_clock::now());
    });
}

bool Server::SessionRegistry::registerSession(SessionId id, SessionRecord record) {
    auto existing = active_users_.find(record.user_id);
    if (existing != active_users_.end() && existing->second != id) {
        return false;
    }
    auto previous = records_.find(id);
    if (previous != records_.end()) {
        active_users_.erase(previous->second.user_id);
    }
    active_users_[record.user_id] = id;
    records_[id] = std::move(record);
    return true;
}

void Server::SessionRegistry::removeSession(SessionId id) {
    auto it = records_.find(id);
    if (it != records_.end()) {
        active_users_.erase(it->second.user_id);
        records_.erase(it);
    }
}

const Server::SessionRecord *Server::SessionRegistry::find(SessionId id) const {
    auto it = records_.find(id);
    if (it == records_.end()) {
        return nullptr;
    }
    return &it->second;
}

bool Server::SessionRegistry::hasUser(const std::string &user_id,
                                      SessionId &session_id) const {
    auto it = active_users_.find(user_id);
    if (it == active_users_.end()) {
        return false;
    }
    session_id = it->second;
    return true;
}

std::shared_ptr<Session> Server::createSession(
    const SessionConfig &config,
    std::chrono::steady_clock::time_point now) {
    auto session = std::make_shared<Session>(next_id_++, config, now);
    sessions_.emplace(session->id(), session);
    admin::LogFields fields;
    fields.session_id = session->id();
    fields.session_trace_id = session->traceId();
    logger_.log("info", "session_created", "Session created", fields);
    return session;
}

void Server::removeSession(SessionId id) {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) {
        admin::LogFields fields;
        fields.session_id = it->second->id();
        fields.session_trace_id = it->second->traceId();
        logger_.log("info", "session_removed", "Session removed", fields);
        it->second->clearUserContext();
        sessions_.erase(it);
    }
    registry_.removeSession(id);
    party_service_.removeMember(id);
    guild_service_.removeMember(id);
}

std::shared_ptr<Session> Server::findSession(SessionId id) const {
    auto it = sessions_.find(id);
    if (it == sessions_.end()) {
        return nullptr;
    }
    return it->second;
}

void Server::tick(std::chrono::steady_clock::time_point now) {
    std::vector<SessionId> to_remove;
    for (const auto &entry : sessions_) {
        if (!entry.second->tick(now)) {
            to_remove.push_back(entry.first);
        }
    }
    for (SessionId id : to_remove) {
        removeSession(id);
    }
}

std::size_t Server::sessionCount() const {
    return sessions_.size();
}

Server::Metrics Server::metrics() const {
    return metrics_;
}

std::chrono::steady_clock::time_point Server::startTime() const {
    return started_at_;
}

std::optional<std::vector<std::uint8_t>> Server::handlePacket(
    Session &session,
    const FrameHeader &header,
    const std::vector<std::uint8_t> &payload,
    std::chrono::steady_clock::time_point now) {
    const auto request_trace_id = admin::StructuredLogger::generateTraceId();
    metrics_.packets_total += 1;
    metrics_.bytes_total += payload.size();
    admin::LogFields received_fields;
    received_fields.session_id = session.id();
    received_fields.session_trace_id = session.traceId();
    received_fields.request_trace_id = request_trace_id;
    received_fields.packet_type = header.type;
    received_fields.protocol_version = header.version;
    received_fields.bytes = payload.size();
    logger_.log("info", "packet_received", "Packet received", received_fields);

    session.onReceive(now);
    session.setProtocolVersion(header.version);

    if (header.version < kMinProtocolVersion || header.version > kMaxProtocolVersion) {
        VersionReject reject;
        reject.min_version = kMinProtocolVersion;
        reject.max_version = kMaxProtocolVersion;
        reject.client_version = header.version;
        std::ostringstream message;
        message << "Unsupported client version " << header.version
                << " (supported " << kMinProtocolVersion << "-"
                << kMaxProtocolVersion << ")";
        reject.message = message.str();
        auto encoded = encodeVersionReject(reject);
        metrics_.error_total += 1;
        admin::LogFields fields = received_fields;
        fields.reason = reject.message;
        logger_.log("warn", "packet_rejected", "Unsupported protocol version",
                    fields);
        return Codec::encode(static_cast<std::uint16_t>(PacketType::VersionReject),
                             header.version,
                             encoded);
    }

    switch (static_cast<PacketType>(header.type)) {
        case PacketType::LoginReq: {
            LoginRequest request;
            if (!decodeLoginRequest(payload, request)) {
                LoginResponse response;
                response.accepted = false;
                response.message = "Malformed login payload";
                auto encoded = encodeLoginResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "login_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            if (request.password != "letmein") {
                LoginResponse response;
                response.accepted = false;
                response.message = "Invalid credentials";
                auto encoded = encodeLoginResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = request.user_id;
                fields.reason = response.message;
                logger_.log("warn", "login_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            SessionId existing_id = 0;
            if (registry_.hasUser(request.user_id, existing_id) &&
                existing_id != session.id()) {
                LoginResponse response;
                response.accepted = false;
                response.message = "User already logged in";
                auto encoded = encodeLoginResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = request.user_id;
                fields.reason = response.message;
                logger_.log("warn", "login_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            auto token = token_service_.issueToken(request.user_id, now);
            Session::UserContext context{request.user_id, token};
            session.attachUserContext(context);
            if (!registry_.registerSession(session.id(), {request.user_id, token})) {
                LoginResponse response;
                response.accepted = false;
                response.message = "User already logged in";
                auto encoded = encodeLoginResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = request.user_id;
                fields.reason = response.message;
                logger_.log("warn", "login_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                     header.version,
                                     encoded);
            }

            LoginResponse response;
            response.accepted = true;
            response.token = token;
            response.message = "Login accepted";
            auto encoded = encodeLoginResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = request.user_id;
            logger_.log("info", "login_success", response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::LoginRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::LogoutReq: {
            LogoutRequest request;
            if (!decodeLogoutRequest(payload, request)) {
                LogoutResponse response;
                response.success = false;
                response.message = "Malformed logout payload";
                auto encoded = encodeLogoutResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "logout_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::LogoutRes),
                                     header.version,
                                     encoded);
            }

            session.clearUserContext();
            registry_.removeSession(session.id());

            LogoutResponse response;
            response.success = true;
            response.message = "Logout successful";
            auto encoded = encodeLogoutResponse(response);
            logger_.log("info", "logout_success", response.message, received_fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::LogoutRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::GuildCreateReq: {
            GuildCreateRequest request;
            if (!decodeGuildCreateRequest(payload, request)) {
                GuildCreateResponse response;
                response.success = false;
                response.message = "Malformed guild create payload";
                auto encoded = encodeGuildCreateResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_create_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildCreateRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                GuildCreateResponse response;
                response.success = false;
                response.message = "Authentication required";
                auto encoded = encodeGuildCreateResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_create_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildCreateRes),
                                     header.version,
                                     encoded);
            }

            auto guild_id = guild_service_.createGuild(session.id(),
                                                       user->user_id,
                                                       request.guild_name);
            GuildCreateResponse response;
            if (!guild_id) {
                response.success = false;
                response.message = "Unable to create guild";
                metrics_.error_total += 1;
            } else {
                response.success = true;
                response.guild_id = *guild_id;
                response.message = "Guild created";
            }
            auto encoded = encodeGuildCreateResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log(response.success ? "info" : "warn",
                        response.success ? "guild_created" : "guild_create_failed",
                        response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildCreateRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::GuildJoinReq: {
            GuildJoinRequest request;
            if (!decodeGuildJoinRequest(payload, request)) {
                GuildJoinResponse response;
                response.success = false;
                response.message = "Malformed guild join payload";
                auto encoded = encodeGuildJoinResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_join_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildJoinRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                GuildJoinResponse response;
                response.success = false;
                response.message = "Authentication required";
                auto encoded = encodeGuildJoinResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_join_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildJoinRes),
                                     header.version,
                                     encoded);
            }

            GuildJoinResponse response;
            response.success = guild_service_.joinGuild(request.guild_id,
                                                        session.id(),
                                                        user->user_id);
            response.message = response.success ? "Joined guild" : "Unable to join guild";
            auto encoded = encodeGuildJoinResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log(response.success ? "info" : "warn",
                        response.success ? "guild_joined" : "guild_join_failed",
                        response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildJoinRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::GuildLeaveReq: {
            GuildLeaveRequest request;
            if (!decodeGuildLeaveRequest(payload, request)) {
                GuildLeaveResponse response;
                response.success = false;
                response.message = "Malformed guild leave payload";
                auto encoded = encodeGuildLeaveResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_leave_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildLeaveRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                GuildLeaveResponse response;
                response.success = false;
                response.message = "Authentication required";
                auto encoded = encodeGuildLeaveResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "guild_leave_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildLeaveRes),
                                     header.version,
                                     encoded);
            }

            auto guild_id = request.guild_id;
            if (guild_id == 0) {
                auto current = guild_service_.guildForMember(session.id());
                if (!current) {
                    GuildLeaveResponse response;
                    response.success = false;
                    response.message = "Not in a guild";
                    auto encoded = encodeGuildLeaveResponse(response);
                    metrics_.error_total += 1;
                    admin::LogFields fields = received_fields;
                    fields.user_id = user->user_id;
                    fields.reason = response.message;
                    logger_.log("warn", "guild_leave_failed", response.message, fields);
                    return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildLeaveRes),
                                         header.version,
                                         encoded);
                }
                guild_id = *current;
            }

            GuildLeaveResponse response;
            response.success = guild_service_.leaveGuild(guild_id, session.id());
            response.message = response.success ? "Left guild" : "Unable to leave guild";
            auto encoded = encodeGuildLeaveResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log(response.success ? "info" : "warn",
                        response.success ? "guild_left" : "guild_leave_failed",
                        response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::GuildLeaveRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::ChatSendReq: {
            ChatSendRequest request;
            if (!decodeChatSendRequest(payload, request)) {
                ChatSendResponse response;
                response.success = false;
                response.message = "Malformed chat payload";
                auto encoded = encodeChatSendResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "chat_send_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::ChatSendRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                ChatSendResponse response;
                response.success = false;
                response.message = "Authentication required";
                auto encoded = encodeChatSendResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "chat_send_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::ChatSendRes),
                                     header.version,
                                     encoded);
            }

            if (request.message.empty()) {
                ChatSendResponse response;
                response.success = false;
                response.message = "Chat message cannot be empty";
                auto encoded = encodeChatSendResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "chat_send_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::ChatSendRes),
                                     header.version,
                                     encoded);
            }

            ChatSendResponse response;
            std::vector<SessionId> recipients;
            if (request.channel == ChatChannel::Global) {
                recipients.reserve(sessions_.size());
                for (const auto &entry : sessions_) {
                    if (entry.second->userContext().has_value()) {
                        recipients.push_back(entry.first);
                    }
                }
                response.success = chat_service_.sendGlobal(session.id(),
                                                            user->user_id,
                                                            request.message,
                                                            recipients);
                response.message = response.success ? "Global chat delivered"
                                                    : "Failed to deliver global chat";
            } else if (request.channel == ChatChannel::Party) {
                std::uint64_t party_id = request.party_id;
                bool can_send = true;
                if (party_id == 0) {
                    auto current_party = party_service_.partyForMember(session.id());
                    if (!current_party) {
                        response.success = false;
                        response.message = "Not in a party";
                        can_send = false;
                        metrics_.error_total += 1;
                    } else {
                        party_id = *current_party;
                    }
                }
                if (can_send) {
                    auto party_info = party_service_.getPartyInfo(party_id);
                    if (!party_info) {
                        response.success = false;
                        response.message = "Party not found";
                        metrics_.error_total += 1;
                    } else {
                        bool is_member = false;
                        recipients.reserve(party_info->members.size());
                        for (const auto &member : party_info->members) {
                            if (member.session_id == session.id()) {
                                is_member = true;
                            }
                            recipients.push_back(member.session_id);
                        }
                        if (!is_member) {
                            response.success = false;
                            response.message = "Not authorized for party chat";
                            metrics_.error_total += 1;
                        } else {
                            response.success = chat_service_.sendParty(session.id(),
                                                                      user->user_id,
                                                                      party_id,
                                                                      request.message,
                                                                      recipients);
                            response.message = response.success
                                                   ? "Party chat delivered"
                                                   : "Failed to deliver party chat";
                            if (!response.success) {
                                metrics_.error_total += 1;
                            }
                        }
                    }
                }
            } else {
                response.success = false;
                response.message = "Unknown chat channel";
                metrics_.error_total += 1;
            }

            auto encoded = encodeChatSendResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log(response.success ? "info" : "warn",
                        response.success ? "chat_sent" : "chat_send_failed",
                        response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::ChatSendRes),
                                 header.version,
                                 encoded);
        }
        default:
            metrics_.error_total += 1;
            admin::LogFields fields = received_fields;
            fields.reason = "Unknown packet type";
            logger_.log("warn", "packet_unhandled", "Unknown packet type",
                        fields);
            break;
    }

    return std::nullopt;
}

const Session::UserContext *Server::sessionUser(SessionId id) const {
    return registry_.find(id);
}

party::PartyService &Server::partyService() {
    return party_service_;
}

bool Server::forceDisconnect(SessionId id,
                             const std::string &reason,
                             const std::string &request_trace_id) {
    auto session = findSession(id);
    if (!session) {
        admin::LogFields fields;
        fields.session_id = id;
        fields.request_trace_id = request_trace_id;
        fields.reason = "Session not found";
        logger_.log("warn", "session_force_disconnect_failed",
                    "Session not found", fields);
        metrics_.error_total += 1;
        return false;
    }

    admin::LogFields fields;
    fields.session_id = id;
    fields.session_trace_id = session->traceId();
    fields.request_trace_id = request_trace_id;
    fields.reason = reason;
    logger_.log("info", "session_force_disconnected",
                "Session force disconnected", fields);
    removeSession(id);
    return true;
}

}  // namespace net
