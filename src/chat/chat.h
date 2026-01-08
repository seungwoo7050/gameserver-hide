#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace chat {

using SessionId = std::uint64_t;

enum class ChatChannel : std::uint16_t {
    Global = 1,
    Party = 2
};

struct ChatMessage {
    ChatChannel channel{ChatChannel::Global};
    std::uint64_t party_id{0};
    SessionId sender_session_id{0};
    std::string sender_user_id;
    std::string text;
};

class ChatService {
public:
    using EventSink = std::function<void(SessionId, const ChatMessage &message)>;

    void setEventSink(EventSink sink);
    bool sendGlobal(SessionId sender_session_id,
                    std::string sender_user_id,
                    std::string message,
                    const std::vector<SessionId> &recipients);
    bool sendParty(SessionId sender_session_id,
                   std::string sender_user_id,
                   std::uint64_t party_id,
                   std::string message,
                   const std::vector<SessionId> &recipients);

private:
    void emitToRecipients(const ChatMessage &message,
                          const std::vector<SessionId> &recipients);

    EventSink event_sink_;
};

}  // namespace chat
