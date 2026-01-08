#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace admin {

struct LogFields {
    std::optional<std::string> trace_id;
    std::optional<std::string> session_trace_id;
    std::optional<std::string> request_trace_id;
    std::optional<std::uint64_t> session_id;
    std::optional<std::uint16_t> packet_type;
    std::optional<std::uint16_t> protocol_version;
    std::optional<std::uint64_t> bytes;
    std::optional<std::string> user_id;
    std::optional<std::string> reason;
};

class StructuredLogger {
public:
    StructuredLogger() = default;

    void log(const std::string &level,
             const std::string &event,
             const std::string &message,
             const LogFields &fields = {});

    static std::string generateTraceId();

private:
    std::mutex mutex_;
};

}  // namespace admin
