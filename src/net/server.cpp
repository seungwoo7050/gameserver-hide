#include "net/server.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <vector>

#include "inventory/cached_inventory_storage.h"
#include "inventory/in_memory_inventory_storage.h"
#include "inventory/mysql_inventory_storage.h"

namespace net {

Server::Server(std::shared_ptr<inventory::InventoryStorage> inventory_storage)
    : inventory_storage_(std::move(inventory_storage)),
      started_at_(std::chrono::steady_clock::now()) {
    if (!inventory_storage_) {
        inventory_storage_ = std::make_shared<inventory::CachedInventoryStorage>(
            std::make_unique<inventory::MySqlInventoryStorage>(),
            std::make_unique<inventory::InMemoryInventoryStorage>());
    }
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
    session_instances_.erase(id);
    session_characters_.erase(id);
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
        case PacketType::SessionReconnectReq: {
            SessionReconnectRequest request;
            if (!decodeSessionReconnectRequest(payload, request)) {
                SessionReconnectResponse response;
                response.success = false;
                response.message = "Malformed reconnect payload";
                auto encoded = encodeSessionReconnectResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "session_reconnect_failed", response.message, fields);
                return Codec::encode(
                    static_cast<std::uint16_t>(PacketType::SessionReconnectRes),
                    header.version,
                    encoded);
            }

            std::string user_id;
            if (!token_service_.validateToken(request.token, now, user_id)) {
                SessionReconnectResponse response;
                response.success = false;
                response.message = "Invalid or expired token";
                auto encoded = encodeSessionReconnectResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "session_reconnect_failed", response.message, fields);
                return Codec::encode(
                    static_cast<std::uint16_t>(PacketType::SessionReconnectRes),
                    header.version,
                    encoded);
            }

            SessionId existing_id = 0;
            std::uint64_t previous_last_seq = 0;
            if (registry_.hasUser(user_id, existing_id) && existing_id != session.id()) {
                auto existing_session = findSession(existing_id);
                if (existing_session) {
                    previous_last_seq = existing_session->lastSeq();
                    party_service_.replaceMemberSession(existing_id, session.id());
                    guild_service_.replaceMemberSession(existing_id, session.id());
                    auto instance_it = session_instances_.find(existing_id);
                    if (instance_it != session_instances_.end()) {
                        session_instances_[session.id()] = instance_it->second;
                        session_instances_.erase(instance_it);
                    }
                    auto character_it = session_characters_.find(existing_id);
                    if (character_it != session_characters_.end()) {
                        session_characters_[session.id()] = character_it->second;
                        session_characters_.erase(character_it);
                    }
                    existing_session->clearUserContext();
                    sessions_.erase(existing_id);
                }
                registry_.removeSession(existing_id);
            }

            Session::UserContext context{user_id, request.token};
            session.attachUserContext(context);
            if (!registry_.registerSession(session.id(), {user_id, request.token})) {
                SessionReconnectResponse response;
                response.success = false;
                response.message = "User already logged in";
                auto encoded = encodeSessionReconnectResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user_id;
                fields.reason = response.message;
                logger_.log("warn", "session_reconnect_failed", response.message, fields);
                return Codec::encode(
                    static_cast<std::uint16_t>(PacketType::SessionReconnectRes),
                    header.version,
                    encoded);
            }

            std::uint64_t restored_last_seq =
                std::max<std::uint64_t>(request.last_seq, previous_last_seq);
            session.setLastSeq(restored_last_seq);

            SessionReconnectResponse response;
            response.success = true;
            response.message = "Reconnect accepted";
            response.session_id = session.id();
            response.resume_from_seq = static_cast<std::uint32_t>(restored_last_seq + 1);
            auto encoded = encodeSessionReconnectResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user_id;
            fields.reason = response.message;
            logger_.log("info", "session_reconnected", response.message, fields);
            return Codec::encode(
                static_cast<std::uint16_t>(PacketType::SessionReconnectRes),
                header.version,
                encoded);
        }
        case PacketType::MatchReq: {
            MatchRequest request;
            if (!decodeMatchRequest(payload, request)) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "MALFORMED";
                response.message = "Malformed match request";
                auto encoded = encodeMatchFoundNotify(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "UNAUTHENTICATED";
                response.message = "Authentication required";
                auto encoded = encodeMatchFoundNotify(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encoded);
            }

            std::uint64_t party_id = request.party_id;
            if (party_id == 0) {
                auto party_for_member = party_service_.partyForMember(session.id());
                if (!party_for_member) {
                    MatchFoundNotify response;
                    response.success = false;
                    response.code = "NO_PARTY";
                    response.message = "Not in a party";
                    auto encoded = encodeMatchFoundNotify(response);
                    metrics_.error_total += 1;
                    admin::LogFields fields = received_fields;
                    fields.user_id = user->user_id;
                    fields.reason = response.message;
                    logger_.log("warn", "match_request_failed", response.message, fields);
                    return Codec::encode(
                        static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                        header.version,
                        encoded);
                }
                party_id = *party_for_member;
            }

            auto party_info = party_service_.getPartyInfo(party_id);
            if (!party_info) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "PARTY_NOT_FOUND";
                response.message = "Party not found";
                auto encoded = encodeMatchFoundNotify(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encoded);
            }

            bool is_member = false;
            for (const auto &member : party_info->members) {
                if (member.session_id == session.id()) {
                    is_member = true;
                    break;
                }
            }
            if (!is_member) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "NOT_PARTY_MEMBER";
                response.message = "Not authorized for match";
                auto encoded = encodeMatchFoundNotify(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encoded);
            }

            match::MatchCandidate candidate;
            candidate.party_id = party_id;
            candidate.mmr = 0;
            candidate.party_size = party_info->members.size();
            candidate.enqueue_time = now;
            if (!match_queue_.enqueue(candidate)) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "QUEUE_REJECTED";
                response.message = "Unable to enqueue for match";
                auto encoded = encodeMatchFoundNotify(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encoded);
            }

            std::optional<std::pair<match::MatchCandidate, match::MatchCandidate>> found =
                match_queue_.findMatch(now);
            std::vector<match::MatchCandidate> matches;
            if (found) {
                matches.push_back(found->first);
                matches.push_back(found->second);
            } else {
                match_queue_.cancel(party_id);
                matches.push_back(candidate);
            }

            std::optional<MatchFoundNotify> response_to_requester;
            for (const auto &match_candidate : matches) {
                auto instance_id = instance_manager_.createInstance(match_candidate.party_id,
                                                                    party_service_);
                if (!instance_id) {
                    MatchFoundNotify response;
                    response.success = false;
                    response.code = "INSTANCE_FAILED";
                    response.message = "Unable to create dungeon instance";
                    metrics_.error_total += 1;
                    admin::LogFields fields = received_fields;
                    fields.user_id = user->user_id;
                    fields.reason = response.message;
                    logger_.log("warn", "match_request_failed", response.message, fields);
                    return Codec::encode(
                        static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                        header.version,
                        encodeMatchFoundNotify(response));
                }

                std::string ticket = admin::StructuredLogger::generateTraceId();
                std::string endpoint = "dungeon.local:7777";
                party_instances_[match_candidate.party_id] = *instance_id;
                instance_tickets_[*instance_id] = ticket;
                std::uniform_int_distribution<std::uint32_t> dist(
                    1, std::numeric_limits<std::uint32_t>::max());
                instance_seeds_[*instance_id] = dist(rng_);

                MatchFoundNotify notify;
                notify.success = true;
                notify.code = "OK";
                notify.message = "Match found";
                notify.party_id = match_candidate.party_id;
                notify.instance_id = *instance_id;
                notify.endpoint = endpoint;
                notify.ticket = ticket;

                auto encoded = encodeMatchFoundNotify(notify);
                auto frame = Codec::encode(
                    static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                    header.version,
                    encoded);

                auto notify_party_info = party_service_.getPartyInfo(match_candidate.party_id);
                if (notify_party_info) {
                    for (const auto &member : notify_party_info->members) {
                        auto member_session = findSession(member.session_id);
                        if (member_session) {
                            session_instances_[member.session_id] = *instance_id;
                            if (!(match_candidate.party_id == party_id &&
                                  member.session_id == session.id())) {
                                member_session->enqueueSend(
                                    frame, std::chrono::steady_clock::now());
                            }
                        }
                    }
                }

                if (match_candidate.party_id == party_id) {
                    response_to_requester = notify;
                }
            }

            if (!response_to_requester) {
                MatchFoundNotify response;
                response.success = false;
                response.code = "MATCH_NOT_FOUND";
                response.message = "Match not found";
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "match_request_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                     header.version,
                                     encodeMatchFoundNotify(response));
            }

            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response_to_requester->message;
            logger_.log("info", "match_found", response_to_requester->message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::MatchFoundNotify),
                                 header.version,
                                 encodeMatchFoundNotify(*response_to_requester));
        }
        case PacketType::DungeonEnterReq: {
            DungeonEnterRequest request;
            if (!decodeDungeonEnterRequest(payload, request)) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "MALFORMED";
                response.message = "Malformed dungeon enter payload";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "UNAUTHENTICATED";
                response.message = "Authentication required";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            auto instance = instance_manager_.getInstance(request.instance_id);
            if (!instance) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "INSTANCE_NOT_FOUND";
                response.message = "Dungeon instance not found";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            auto ticket_it = instance_tickets_.find(request.instance_id);
            if (ticket_it == instance_tickets_.end() || ticket_it->second != request.ticket) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "INVALID_TICKET";
                response.message = "Invalid enter ticket";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            auto party_info = party_service_.getPartyInfo(instance->party_id);
            if (!party_info) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "PARTY_NOT_FOUND";
                response.message = "Party not found for instance";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            bool is_member = false;
            for (const auto &member : party_info->members) {
                if (member.session_id == session.id()) {
                    is_member = true;
                    break;
                }
            }
            if (!is_member) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "NOT_PARTY_MEMBER";
                response.message = "Not authorized for instance";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            if (!instance_manager_.requestTransition(request.instance_id,
                                                     dungeon::InstanceState::Ready,
                                                     party_service_)) {
                DungeonEnterResponse response;
                response.success = false;
                response.code = "INVALID_STATE";
                response.message = "Dungeon not ready to enter";
                auto encoded = encodeDungeonEnterResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_enter_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                     header.version,
                                     encoded);
            }

            session_characters_[session.id()] = request.char_id;
            session_instances_[session.id()] = request.instance_id;

            DungeonEnterResponse response;
            response.success = true;
            response.code = "OK";
            response.message = "Dungeon entry accepted";
            response.state = DungeonState::Ready;
            response.seed = instance_seeds_[request.instance_id];
            auto encoded = encodeDungeonEnterResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log("info", "dungeon_entered", response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonEnterRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::DungeonResultNotify: {
            DungeonResultNotify request;
            if (!decodeDungeonResultNotify(payload, request)) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "MALFORMED";
                response.message = "Malformed dungeon result payload";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "UNAUTHENTICATED";
                response.message = "Authentication required";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            auto instance_it = session_instances_.find(session.id());
            if (instance_it == session_instances_.end()) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "NO_INSTANCE";
                response.message = "No active dungeon instance";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            auto instance = instance_manager_.getInstance(instance_it->second);
            if (!instance) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "INSTANCE_NOT_FOUND";
                response.message = "Dungeon instance missing";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            auto grant_it = instance_reward_grants_.find(instance_it->second);
            if (grant_it != instance_reward_grants_.end()) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "REWARD_DUPLICATE";
                response.message = "Reward grant already processed";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            dungeon::InstanceState next_state =
                request.result == DungeonResultType::Clear ? dungeon::InstanceState::Clear
                                                           : dungeon::InstanceState::Fail;
            if (!instance_manager_.requestTransition(instance_it->second,
                                                     next_state,
                                                     party_service_)) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "INVALID_STATE";
                response.message = "Dungeon state transition rejected";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            auto char_it = session_characters_.find(session.id());
            if (char_it == session_characters_.end()) {
                DungeonResultResponse response;
                response.success = false;
                response.code = "CHAR_NOT_SET";
                response.message = "Character not registered for session";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            std::vector<reward::RewardItem> reward_items;
            reward_items.reserve(request.rewards.size());
            for (const auto &item : request.rewards) {
                reward_items.push_back(reward::RewardItem{item.item_id, item.count});
            }

            reward::Inventory reward_inventory;
            auto grant_id = next_reward_grant_id_++;
            auto grant_result = reward_service_.grantRewardsDetailed(
                reward_inventory, grant_id, reward_items);
            if (grant_result != reward::RewardService::GrantResult::Completed) {
                DungeonResultResponse response;
                response.success = false;
                response.code = grant_result == reward::RewardService::GrantResult::Duplicate
                                    ? "REWARD_DUPLICATE"
                                    : "REWARD_FAILED";
                response.message = grant_result == reward::RewardService::GrantResult::Duplicate
                                       ? "Reward grant already processed"
                                       : "Reward grant failed";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }

            bool inventory_ok = true;
            auto inventory_tx = inventory_storage_->beginTransaction();
            for (const auto &item : request.rewards) {
                if (!inventory_storage_->addItem(char_it->second,
                                                 item.item_id,
                                                 item.count,
                                                 "dungeon_reward")) {
                    inventory_ok = false;
                    break;
                }
            }
            if (!inventory_ok) {
                inventory_storage_->rollbackTransaction(inventory_tx);
                DungeonResultResponse response;
                response.success = false;
                response.code = "INVENTORY_FAILED";
                response.message = "Failed to update inventory";
                response.summary = "result rejected";
                auto encoded = encodeDungeonResultResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.user_id = user->user_id;
                fields.reason = response.message;
                logger_.log("warn", "dungeon_result_failed", response.message, fields);
                return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                     header.version,
                                     encoded);
            }
            inventory_storage_->commitTransaction(inventory_tx);

            DungeonResultResponse response;
            response.success = true;
            response.code = "OK";
            response.message = "Dungeon result recorded";
            response.summary = "result recorded";
            instance_reward_grants_[instance_it->second] = grant_id;
            auto encoded = encodeDungeonResultResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log("info", "dungeon_result_recorded", response.message, fields);
            return Codec::encode(static_cast<std::uint16_t>(PacketType::DungeonResultRes),
                                 header.version,
                                 encoded);
        }
        case PacketType::InventoryUpdateNotify: {
            InventoryUpdateNotify request;
            if (!decodeInventoryUpdateNotify(payload, request)) {
                InventoryUpdateResponse response;
                response.success = false;
                response.code = "MALFORMED";
                response.message = "Malformed inventory update payload";
                auto encoded = encodeInventoryUpdateResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "inventory_update_failed", response.message, fields);
                return Codec::encode(
                    static_cast<std::uint16_t>(PacketType::InventoryUpdateRes),
                    header.version,
                    encoded);
            }

            const auto *user = session.userContext().has_value()
                                   ? &session.userContext().value()
                                   : nullptr;
            if (!user) {
                InventoryUpdateResponse response;
                response.success = false;
                response.code = "UNAUTHENTICATED";
                response.message = "Authentication required";
                auto encoded = encodeInventoryUpdateResponse(response);
                metrics_.error_total += 1;
                admin::LogFields fields = received_fields;
                fields.reason = response.message;
                logger_.log("warn", "inventory_update_failed", response.message, fields);
                return Codec::encode(
                    static_cast<std::uint16_t>(PacketType::InventoryUpdateRes),
                    header.version,
                    encoded);
            }

            bool inventory_ok = true;
            auto inventory_tx = inventory_storage_->beginTransaction();
            for (const auto &item : request.items) {
                if (!inventory_storage_->addItem(request.char_id,
                                                 item.item_id,
                                                 item.count,
                                                 "inventory_update")) {
                    inventory_ok = false;
                    break;
                }
            }
            if (!inventory_ok) {
                inventory_storage_->rollbackTransaction(inventory_tx);
            } else {
                inventory_storage_->commitTransaction(inventory_tx);
            }

            InventoryUpdateResponse response;
            response.success = inventory_ok;
            response.code = inventory_ok ? "OK" : "INVENTORY_FAILED";
            response.message =
                inventory_ok ? "Inventory updated" : "Failed to update inventory";
            response.inventory_version =
                inventory_storage_->changeLog(request.char_id).size();
            auto encoded = encodeInventoryUpdateResponse(response);
            admin::LogFields fields = received_fields;
            fields.user_id = user->user_id;
            fields.reason = response.message;
            logger_.log(response.success ? "info" : "warn",
                        response.success ? "inventory_updated"
                                         : "inventory_update_failed",
                        response.message,
                        fields);
            return Codec::encode(
                static_cast<std::uint16_t>(PacketType::InventoryUpdateRes),
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

dungeon::InstanceManager &Server::instanceManager() {
    return instance_manager_;
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
