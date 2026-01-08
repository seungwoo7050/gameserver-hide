#include "net/codec.h"
#include "net/protocol.h"
#include "net/server.h"

#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

struct Options {
    std::size_t sessions{24};
    std::size_t requests_per_session{1};
    std::size_t concurrency{6};
    std::size_t send_queue_limit_bytes{2048};
    std::size_t overflow_payload_bytes{4096};
    std::size_t overflow_burst{3};
    std::string log_path{"docs/load_run.log"};
    std::string summary_path{"docs/load_summary.md"};
};

struct OverflowStats {
    std::size_t sessions{0};
    std::size_t attempts{0};
    std::size_t accepted{0};
    std::size_t dropped{0};
    std::size_t disconnected{0};
    std::size_t max_queued_bytes{0};
};

struct ValidationResults {
    std::size_t match_found_entries{0};
    std::size_t match_found_unique_sessions{0};
    std::size_t match_found_duplicates{0};
    std::size_t queue_overflow_warnings{0};
    std::size_t overflow_disconnects{0};
    bool duplicate_check_passed{false};
    bool overflow_policy_logged{false};
};

struct SessionBundle {
    std::shared_ptr<net::Session> session;
    std::uint64_t party_id{0};
    std::string user_id;
    net::OverflowPolicy policy{net::OverflowPolicy::DropNewest};
};

void printUsage(const char *argv0) {
    std::cout
        << "Usage: " << argv0
        << " [--sessions N] [--requests-per-session N] [--concurrency N]"
        << " [--send-queue-limit BYTES] [--overflow-payload BYTES]"
        << " [--overflow-burst N] [--log-path PATH] [--summary-path PATH]\n";
}

std::optional<std::size_t> parseSize(const char *value) {
    try {
        std::size_t idx = 0;
        std::string text(value);
        std::size_t result = std::stoull(text, &idx, 10);
        if (idx != text.size()) {
            return std::nullopt;
        }
        return result;
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

Options parseArgs(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        auto nextValue = [&]() -> const char * {
            if (i + 1 >= argc) {
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--sessions") {
            if (auto value = parseSize(nextValue())) {
                options.sessions = *value;
            }
        } else if (arg == "--requests-per-session") {
            if (auto value = parseSize(nextValue())) {
                options.requests_per_session = *value;
            }
        } else if (arg == "--concurrency") {
            if (auto value = parseSize(nextValue())) {
                options.concurrency = *value;
            }
        } else if (arg == "--send-queue-limit") {
            if (auto value = parseSize(nextValue())) {
                options.send_queue_limit_bytes = *value;
            }
        } else if (arg == "--overflow-payload") {
            if (auto value = parseSize(nextValue())) {
                options.overflow_payload_bytes = *value;
            }
        } else if (arg == "--overflow-burst") {
            if (auto value = parseSize(nextValue())) {
                options.overflow_burst = *value;
            }
        } else if (arg == "--log-path") {
            if (auto value = nextValue()) {
                options.log_path = value;
            }
        } else if (arg == "--summary-path") {
            if (auto value = nextValue()) {
                options.summary_path = value;
            }
        }
    }
    return options;
}

net::FrameHeader makeHeader(std::uint16_t type, std::uint16_t version,
                            std::size_t length) {
    net::FrameHeader header{};
    header.length = static_cast<std::uint32_t>(length);
    header.type = type;
    header.version = version;
    return header;
}

std::string policyName(net::OverflowPolicy policy) {
    switch (policy) {
        case net::OverflowPolicy::DropNewest:
            return "DropNewest";
        case net::OverflowPolicy::DropOldest:
            return "DropOldest";
        case net::OverflowPolicy::Disconnect:
            return "Disconnect";
    }
    return "Unknown";
}

void ensureParentDir(const std::string &path) {
    if (path.empty()) {
        return;
    }
    std::filesystem::path target(path);
    auto parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

std::optional<std::uint64_t> parseNumberField(const std::string &line,
                                              const std::string &field) {
    std::string key = "\"" + field + "\":";
    auto pos = line.find(key);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos += key.size();
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
        ++pos;
    }
    std::size_t end = pos;
    while (end < line.size() && std::isdigit(static_cast<unsigned char>(line[end]))) {
        ++end;
    }
    if (end == pos) {
        return std::nullopt;
    }
    try {
        return std::stoull(line.substr(pos, end - pos));
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

ValidationResults validateLogs(const std::string &log_path) {
    ValidationResults results;
    if (log_path.empty()) {
        return results;
    }

    std::ifstream log_file(log_path);
    if (!log_file) {
        return results;
    }

    std::unordered_set<std::uint64_t> match_sessions;
    std::string line;
    while (std::getline(log_file, line)) {
        if (line.find("\"event\":\"match_found\"") != std::string::npos) {
            results.match_found_entries += 1;
            if (auto session_id = parseNumberField(line, "session_id")) {
                if (!match_sessions.insert(*session_id).second) {
                    results.match_found_duplicates += 1;
                }
            }
            continue;
        }
        if (line.find("\"event\":\"session_queue_overflow\"") != std::string::npos) {
            results.queue_overflow_warnings += 1;
            continue;
        }
        if (line.find("\"event\":\"session_disconnected\"") != std::string::npos &&
            line.find("\"reason\":\"send queue overflow\"") != std::string::npos) {
            results.overflow_disconnects += 1;
        }
    }

    results.match_found_unique_sessions = match_sessions.size();
    results.duplicate_check_passed = results.match_found_duplicates == 0;
    results.overflow_policy_logged =
        results.queue_overflow_warnings > 0 || results.overflow_disconnects > 0;
    return results;
}

}  // namespace

int main(int argc, char **argv) {
    Options options = parseArgs(argc, argv);
    ensureParentDir(options.log_path);
    ensureParentDir(options.summary_path);

    std::ofstream log_file;
    std::streambuf *original_buf = std::cout.rdbuf();
    if (!options.log_path.empty()) {
        log_file.open(options.log_path, std::ios::out | std::ios::trunc);
        if (log_file) {
            std::cout.rdbuf(log_file.rdbuf());
        }
    }

    net::Server server;
    std::mutex server_mutex;
    std::vector<SessionBundle> bundles;
    bundles.reserve(options.sessions);

    auto now = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < options.sessions; ++i) {
        net::SessionConfig config;
        config.send_queue_limit_bytes = options.send_queue_limit_bytes;
        config.rate_limit_capacity = 1e9;
        config.rate_limit_refill_per_sec = 1e9;
        switch (i % 3) {
            case 0:
                config.overflow_policy = net::OverflowPolicy::DropNewest;
                break;
            case 1:
                config.overflow_policy = net::OverflowPolicy::DropOldest;
                break;
            default:
                config.overflow_policy = net::OverflowPolicy::Disconnect;
                break;
        }

        auto session = server.createSession(config, now);
        std::string user_id = "load_user_" + std::to_string(i + 1);
        net::LoginRequest login;
        login.user_id = user_id;
        login.password = "letmein";
        auto login_payload = net::encodeLoginRequest(login);
        auto login_header = makeHeader(
            static_cast<std::uint16_t>(net::PacketType::LoginReq),
            net::kMaxProtocolVersion,
            login_payload.size());
        {
            std::lock_guard<std::mutex> lock(server_mutex);
            server.handlePacket(*session, login_header, login_payload,
                                std::chrono::steady_clock::now());
        }

        auto party_id = server.partyService().createParty(session->id(), user_id);
        if (!party_id.has_value()) {
            std::cerr << "Failed to create party for session " << session->id()
                      << std::endl;
            continue;
        }

        bundles.push_back(SessionBundle{session, *party_id, user_id,
                                        config.overflow_policy});
    }

    std::atomic<std::size_t> match_successes{0};
    std::atomic<std::size_t> match_failures{0};

    auto issueMatch = [&](SessionBundle &bundle) {
        net::MatchRequest request;
        request.party_id = bundle.party_id;
        request.dungeon_id = 2001;
        request.difficulty = "normal";
        auto payload = net::encodeMatchRequest(request);
        auto header = makeHeader(
            static_cast<std::uint16_t>(net::PacketType::MatchReq),
            net::kMaxProtocolVersion,
            payload.size());

        std::optional<std::vector<std::uint8_t>> response;
        {
            std::lock_guard<std::mutex> lock(server_mutex);
            response = server.handlePacket(*bundle.session, header, payload,
                                           std::chrono::steady_clock::now());
        }
        if (!response.has_value()) {
            match_failures.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        net::FrameDecoder decoder;
        decoder.append(*response);
        net::FrameHeader response_header{};
        std::vector<std::uint8_t> response_payload;
        if (!decoder.nextFrame(response_header, response_payload)) {
            match_failures.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (response_header.type !=
            static_cast<std::uint16_t>(net::PacketType::MatchFoundNotify)) {
            match_failures.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        net::MatchFoundNotify notify;
        if (!net::decodeMatchFoundNotify(response_payload, notify) || !notify.success) {
            match_failures.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        match_successes.fetch_add(1, std::memory_order_relaxed);
    };

    std::vector<std::future<void>> futures;
    futures.reserve(options.sessions * options.requests_per_session);
    std::size_t in_flight = 0;
    for (std::size_t r = 0; r < options.requests_per_session; ++r) {
        for (auto &bundle : bundles) {
            futures.emplace_back(std::async(std::launch::async,
                                            [&issueMatch, &bundle]() {
                                                issueMatch(bundle);
                                            }));
            ++in_flight;
            if (in_flight >= options.concurrency) {
                for (auto &future : futures) {
                    future.get();
                }
                futures.clear();
                in_flight = 0;
            }
        }
    }
    for (auto &future : futures) {
        future.get();
    }

    std::vector<std::uint8_t> overflow_payload(options.overflow_payload_bytes, 0xAB);
    std::map<net::OverflowPolicy, OverflowStats> overflow_stats;

    for (auto &bundle : bundles) {
        auto &stats = overflow_stats[bundle.policy];
        stats.sessions += 1;
        for (std::size_t attempt = 0; attempt < options.overflow_burst; ++attempt) {
            stats.attempts += 1;
            bool accepted = bundle.session->enqueueSend(
                overflow_payload, std::chrono::steady_clock::now());
            if (accepted) {
                stats.accepted += 1;
            } else {
                stats.dropped += 1;
            }
            if (!bundle.session->connected()) {
                stats.disconnected += 1;
                break;
            }
        }
        stats.max_queued_bytes = std::max(stats.max_queued_bytes,
                                          bundle.session->queuedBytes());
    }

    auto metrics = server.metrics();
    auto total_requests = options.sessions * options.requests_per_session;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - now);

    if (log_file) {
        log_file.flush();
        log_file.close();
    }

    std::ostringstream summary;
    summary << "# Load Test Summary\n\n";
    summary << "Generated by `scripts/load_match_sim.cpp`.\n\n";
    summary << "## Command\n";
    summary << "```bash\n";
    summary << "./build/dungeonhub_load_sim";
    summary << " --sessions " << options.sessions;
    summary << " --requests-per-session " << options.requests_per_session;
    summary << " --concurrency " << options.concurrency;
    summary << " --send-queue-limit " << options.send_queue_limit_bytes;
    summary << " --overflow-payload " << options.overflow_payload_bytes;
    summary << " --overflow-burst " << options.overflow_burst;
    if (!options.log_path.empty()) {
        summary << " --log-path " << options.log_path;
    }
    if (!options.summary_path.empty()) {
        summary << " --summary-path " << options.summary_path;
    }
    summary << "\n```\n\n";
    summary << "## Match Results\n";
    summary << "- Sessions opened: " << options.sessions << "\n";
    summary << "- Match requests: " << total_requests << "\n";
    summary << "- Match successes: " << match_successes.load() << "\n";
    summary << "- Match failures: " << match_failures.load() << "\n";
    summary << "- Duration: " << duration.count() << " ms\n\n";

    summary << "## Send Queue Overflow\n";
    for (const auto &entry : overflow_stats) {
        summary << "- Policy: " << policyName(entry.first) << "\n";
        summary << "  - Sessions: " << entry.second.sessions << "\n";
        summary << "  - Attempts: " << entry.second.attempts << "\n";
        summary << "  - Accepted: " << entry.second.accepted << "\n";
        summary << "  - Dropped: " << entry.second.dropped << "\n";
        summary << "  - Disconnected: " << entry.second.disconnected << "\n";
        summary << "  - Max queued bytes: " << entry.second.max_queued_bytes << "\n";
    }

    summary << "\n## Server Metrics\n";
    summary << "- Packets total: " << metrics.packets_total << "\n";
    summary << "- Bytes total: " << metrics.bytes_total << "\n";
    summary << "- Errors total: " << metrics.error_total << "\n\n";

    ValidationResults validation = validateLogs(options.log_path);
    summary << "## Validation\n";
    if (!options.log_path.empty()) {
        summary << "- Match duplicate check (log)\n";
        summary << "  - Match found entries: " << validation.match_found_entries
                << "\n";
        summary << "  - Unique sessions matched: "
                << validation.match_found_unique_sessions << "\n";
        summary << "  - Duplicate matches: " << validation.match_found_duplicates
                << "\n";
        summary << "  - Status: "
                << (validation.duplicate_check_passed ? "PASS" : "FAIL") << "\n";
        summary << "- Overflow policy log check\n";
        summary << "  - Queue overflow warnings: "
                << validation.queue_overflow_warnings << "\n";
        summary << "  - Overflow disconnects: " << validation.overflow_disconnects
                << "\n";
        summary << "  - Status: "
                << (validation.overflow_policy_logged ? "PASS" : "FAIL") << "\n";
    } else {
        summary << "- Validation skipped (no log path configured).\n";
    }

    if (!options.log_path.empty()) {
        summary << "## Logs\n";
        summary << "- Log output: `" << options.log_path << "`\n";
    }

    if (!options.summary_path.empty()) {
        std::ofstream summary_file(options.summary_path,
                                   std::ios::out | std::ios::trunc);
        if (summary_file) {
            summary_file << summary.str();
        }
    }

    std::cout.rdbuf(original_buf);
    std::cout << summary.str();

    return 0;
}
