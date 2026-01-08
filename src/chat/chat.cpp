#include "chat/chat.h"

namespace chat {

void ChatService::setEventSink(EventSink sink) {
    event_sink_ = std::move(sink);
}

bool ChatService::sendGlobal(SessionId sender_session_id,
                             std::string sender_user_id,
                             std::string message,
                             const std::vector<SessionId> &recipients) {
    if (!event_sink_ || message.empty() || recipients.empty()) {
        return false;
    }

    ChatMessage payload;
    payload.channel = ChatChannel::Global;
    payload.sender_session_id = sender_session_id;
    payload.sender_user_id = std::move(sender_user_id);
    payload.text = std::move(message);
    emitToRecipients(payload, recipients);
    return true;
}

bool ChatService::sendParty(SessionId sender_session_id,
                            std::string sender_user_id,
                            std::uint64_t party_id,
                            std::string message,
                            const std::vector<SessionId> &recipients) {
    if (!event_sink_ || message.empty() || recipients.empty()) {
        return false;
    }

    ChatMessage payload;
    payload.channel = ChatChannel::Party;
    payload.party_id = party_id;
    payload.sender_session_id = sender_session_id;
    payload.sender_user_id = std::move(sender_user_id);
    payload.text = std::move(message);
    emitToRecipients(payload, recipients);
    return true;
}

void ChatService::emitToRecipients(const ChatMessage &message,
                                   const std::vector<SessionId> &recipients) {
    if (!event_sink_) {
        return;
    }
    for (SessionId recipient : recipients) {
        event_sink_(recipient, message);
    }
}

}  // namespace chat
