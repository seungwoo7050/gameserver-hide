#include "admin/admin.h"

#include "net/server.h"

#include <chrono>

namespace admin {

AdminService::AdminService(net::Server &server) : server_(server) {}

AdminStatus AdminService::getStatus() const {
    AdminStatus status;
    auto metrics = server_.metrics();
    status.active_sessions = server_.sessionCount();
    status.packets_total = metrics.packets_total;
    status.bytes_total = metrics.bytes_total;
    status.error_total = metrics.error_total;
    status.uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - server_.startTime());

    LogFields fields;
    fields.request_trace_id = StructuredLogger::generateTraceId();
    logger_.log("info", "admin_status", "Admin status requested", fields);

    return status;
}

bool AdminService::forceTerminateSession(std::uint64_t session_id,
                                         const std::string &reason) {
    LogFields fields;
    fields.request_trace_id = StructuredLogger::generateTraceId();
    fields.session_id = session_id;
    fields.reason = reason;

    auto session = server_.findSession(session_id);
    if (!session) {
        logger_.log("warn", "admin_force_disconnect", "Session not found",
                    fields);
        return false;
    }

    fields.session_trace_id = session->traceId();
    logger_.log("info", "admin_force_disconnect", "Admin terminating session",
                fields);
    return server_.forceDisconnect(session_id, reason, *fields.request_trace_id);
}

}  // namespace admin
