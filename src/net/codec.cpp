#include "net/codec.h"

#include <algorithm>

namespace net {

namespace {

std::uint32_t read_u32(const std::uint8_t *data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint16_t read_u16(const std::uint8_t *data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

void write_u32(std::uint32_t value, std::uint8_t *out) {
    out[0] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
    out[1] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    out[2] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[3] = static_cast<std::uint8_t>(value & 0xFF);
}

void write_u16(std::uint16_t value, std::uint8_t *out) {
    out[0] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[1] = static_cast<std::uint8_t>(value & 0xFF);
}

}  // namespace

std::vector<std::uint8_t> Codec::encode(std::uint16_t type,
                                        std::uint16_t version,
                                        std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> buffer(kHeaderSize + payload.size());
    write_u32(static_cast<std::uint32_t>(payload.size()), buffer.data());
    write_u16(type, buffer.data() + 4);
    write_u16(version, buffer.data() + 6);
    std::copy(payload.begin(), payload.end(), buffer.begin() + kHeaderSize);
    return buffer;
}

void FrameDecoder::append(std::span<const std::uint8_t> data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());
}

bool FrameDecoder::nextFrame(FrameHeader &header, std::vector<std::uint8_t> &payload) {
    if (buffer_.size() < Codec::kHeaderSize) {
        return false;
    }

    header.length = read_u32(buffer_.data());
    header.type = read_u16(buffer_.data() + 4);
    header.version = read_u16(buffer_.data() + 6);

    if (buffer_.size() < Codec::kHeaderSize + header.length) {
        return false;
    }

    payload.assign(buffer_.begin() + Codec::kHeaderSize,
                   buffer_.begin() + Codec::kHeaderSize + header.length);
    buffer_.erase(buffer_.begin(),
                  buffer_.begin() + Codec::kHeaderSize + header.length);
    return true;
}

}  // namespace net
