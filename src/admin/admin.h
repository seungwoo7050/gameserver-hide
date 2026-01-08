#pragma once

#include "admin/logging.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace net {
class Server;
}

namespace admin {

struct AdminStatus {
    std::size_t active_sessions{0};
    std::uint64_t packets_total{0};
    std::uint64_t bytes_total{0};
    std::uint64_t error_total{0};
    std::chrono::seconds uptime{0};
};

class AdminService {
public:
    explicit AdminService(net::Server &server);

    AdminStatus getStatus() const;
    bool forceTerminateSession(std::uint64_t session_id,
                               const std::string &reason);

private:
    net::Server &server_;
    mutable StructuredLogger logger_{};
};

}  // namespace admin
