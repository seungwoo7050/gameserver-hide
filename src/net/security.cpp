#include "net/security.h"

#include <array>

namespace net {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t fnv1aUpdate(std::uint64_t hash, std::span<const std::uint8_t> data) {
    for (std::uint8_t byte : data) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    return hash;
}

std::array<std::uint8_t, 8> toBytes(std::uint64_t value) {
    std::array<std::uint8_t, 8> out{};
    for (int i = 7; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return out;
}

std::array<std::uint8_t, 4> toBytes(std::uint32_t value) {
    std::array<std::uint8_t, 4> out{};
    for (int i = 3; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(value & 0xFF);
        value >>= 8;
    }
    return out;
}

void writeU32(std::uint32_t value, std::vector<std::uint8_t> &out) {
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

void writeU64(std::uint64_t value, std::vector<std::uint8_t> &out) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
    }
}

bool readU32(const std::vector<std::uint8_t> &payload,
             std::size_t &offset,
             std::uint32_t &out) {
    if (offset + 4 > payload.size()) {
        return false;
    }
    out = (static_cast<std::uint32_t>(payload[offset]) << 24) |
          (static_cast<std::uint32_t>(payload[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(payload[offset + 2]) << 8) |
          static_cast<std::uint32_t>(payload[offset + 3]);
    offset += 4;
    return true;
}

bool readU64(const std::vector<std::uint8_t> &payload,
             std::size_t &offset,
             std::uint64_t &out) {
    if (offset + 8 > payload.size()) {
        return false;
    }
    out = 0;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | payload[offset + static_cast<std::size_t>(i)];
    }
    offset += 8;
    return true;
}

}  // namespace

std::array<std::uint8_t, 16> computeSignature(std::string_view key,
                                              std::uint32_t seq,
                                              std::uint64_t nonce,
                                              std::span<const std::uint8_t> payload) {
    auto key_bytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t *>(key.data()), key.size());
    auto seq_bytes = toBytes(seq);
    auto nonce_bytes = toBytes(nonce);

    std::uint64_t hash1 = fnv1aUpdate(kFnvOffset, key_bytes);
    hash1 = fnv1aUpdate(hash1, seq_bytes);
    hash1 = fnv1aUpdate(hash1, nonce_bytes);
    hash1 = fnv1aUpdate(hash1, payload);

    std::uint64_t hash2 = fnv1aUpdate(kFnvOffset ^ 0x9e3779b97f4a7c15ULL, key_bytes);
    hash2 = fnv1aUpdate(hash2, nonce_bytes);
    hash2 = fnv1aUpdate(hash2, seq_bytes);
    hash2 = fnv1aUpdate(hash2, payload);

    std::array<std::uint8_t, 16> out{};
    auto hash1_bytes = toBytes(hash1);
    auto hash2_bytes = toBytes(hash2);
    std::copy(hash1_bytes.begin(), hash1_bytes.end(), out.begin());
    std::copy(hash2_bytes.begin(), hash2_bytes.end(), out.begin() + 8);
    return out;
}

bool verifySignature(std::string_view key,
                     const SecurityHeader &header,
                     std::span<const std::uint8_t> payload) {
    auto expected = computeSignature(key, header.seq, header.nonce, payload);
    return expected == header.signature;
}

std::vector<std::uint8_t> wrapSecurePayload(std::uint32_t seq,
                                            std::uint64_t nonce,
                                            std::string_view key,
                                            std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> out;
    out.reserve(kSecurityHeaderSize + payload.size());
    auto signature = computeSignature(key, seq, nonce, payload);
    writeU32(seq, out);
    writeU64(nonce, out);
    out.insert(out.end(), signature.begin(), signature.end());
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool unwrapSecurePayload(const std::vector<std::uint8_t> &payload,
                         SecurityHeader &header,
                         std::vector<std::uint8_t> &inner_payload) {
    std::size_t offset = 0;
    if (!readU32(payload, offset, header.seq)) {
        return false;
    }
    if (!readU64(payload, offset, header.nonce)) {
        return false;
    }
    if (offset + header.signature.size() > payload.size()) {
        return false;
    }
    std::copy(payload.begin() + offset,
              payload.begin() + offset + header.signature.size(),
              header.signature.begin());
    offset += header.signature.size();
    inner_payload.assign(payload.begin() + offset, payload.end());
    return true;
}

}  // namespace net
