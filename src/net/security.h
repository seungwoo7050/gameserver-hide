#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace net {

struct SecurityHeader {
    std::uint32_t seq{0};
    std::uint64_t nonce{0};
    std::array<std::uint8_t, 16> signature{};
};

constexpr std::size_t kSecurityHeaderSize =
    sizeof(std::uint32_t) + sizeof(std::uint64_t) + 16;

std::array<std::uint8_t, 16> computeSignature(std::string_view key,
                                              std::uint32_t seq,
                                              std::uint64_t nonce,
                                              std::span<const std::uint8_t> payload);
bool verifySignature(std::string_view key,
                     const SecurityHeader &header,
                     std::span<const std::uint8_t> payload);

std::vector<std::uint8_t> wrapSecurePayload(std::uint32_t seq,
                                            std::uint64_t nonce,
                                            std::string_view key,
                                            std::span<const std::uint8_t> payload);

bool unwrapSecurePayload(const std::vector<std::uint8_t> &payload,
                         SecurityHeader &header,
                         std::vector<std::uint8_t> &inner_payload);

}  // namespace net
