#include "admin/logging.h"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace admin {
namespace {

std::string jsonEscape(const std::string &value) {
    std::ostringstream escaped;
    for (char ch : value) {
        switch (ch) {
            case '"':
                escaped << "\\\"";
                break;
            case '\\':
                escaped << "\\\\";
                break;
            case '\b':
                escaped << "\\b";
                break;
            case '\f':
                escaped << "\\f";
                break;
            case '\n':
                escaped << "\\n";
                break;
            case '\r':
                escaped << "\\r";
                break;
            case '\t':
                escaped << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    escaped << "\\u"
                            << std::hex << std::setw(4) << std::setfill('0')
                            << static_cast<int>(static_cast<unsigned char>(ch))
                            << std::dec;
                } else {
                    escaped << ch;
                }
                break;
        }
    }
    return escaped.str();
}

std::string isoTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#if defined(_WIN32)
    gmtime_s(&utc_tm, &time);
#else
    gmtime_r(&time, &utc_tm);
#endif
    std::ostringstream stamp;
    stamp << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return stamp.str();
}

template <typename T>
void appendNumber(std::ostringstream &out,
                  const char *key,
                  const std::optional<T> &value,
                  bool &first) {
    if (!value.has_value()) {
        return;
    }
    if (!first) {
        out << ',';
    }
    first = false;
    out << '"' << key << "\":" << *value;
}

void appendString(std::ostringstream &out,
                  const char *key,
                  const std::optional<std::string> &value,
                  bool &first) {
    if (!value.has_value() || value->empty()) {
        return;
    }
    if (!first) {
        out << ',';
    }
    first = false;
    out << '"' << key << "\":\"" << jsonEscape(*value) << '\"';
}

}  // namespace

void StructuredLogger::log(const std::string &level,
                           const std::string &event,
                           const std::string &message,
                           const LogFields &fields) {
    std::ostringstream entry;
    entry << '{';
    bool first = true;
    appendString(entry, "timestamp", std::optional<std::string>{isoTimestamp()},
                 first);
    appendString(entry, "level", std::optional<std::string>{level}, first);
    appendString(entry, "event", std::optional<std::string>{event}, first);
    appendString(entry, "message", std::optional<std::string>{message}, first);
    appendString(entry, "trace_id", fields.trace_id, first);
    appendString(entry, "session_trace_id", fields.session_trace_id, first);
    appendString(entry, "request_trace_id", fields.request_trace_id, first);
    appendNumber(entry, "session_id", fields.session_id, first);
    appendNumber(entry, "packet_type", fields.packet_type, first);
    appendNumber(entry, "protocol_version", fields.protocol_version, first);
    appendNumber(entry, "bytes", fields.bytes, first);
    appendString(entry, "user_id", fields.user_id, first);
    appendString(entry, "reason", fields.reason, first);
    entry << '}';

    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << entry.str() << std::endl;
}

std::string StructuredLogger::generateTraceId() {
    std::array<unsigned char, 16> bytes{};
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto &byte : bytes) {
        byte = static_cast<unsigned char>(dist(generator));
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

}  // namespace admin
