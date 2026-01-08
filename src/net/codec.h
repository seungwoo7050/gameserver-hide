#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace net {

struct FrameHeader {
    std::uint32_t length;
    std::uint16_t type;
    std::uint16_t version;
};

class Codec {
public:
    static constexpr std::size_t kHeaderSize = 8;

    static std::vector<std::uint8_t> encode(std::uint16_t type,
                                            std::uint16_t version,
                                            std::span<const std::uint8_t> payload);
};

class FrameDecoder {
public:
    void append(std::span<const std::uint8_t> data);
    bool nextFrame(FrameHeader &header, std::vector<std::uint8_t> &payload);

private:
    std::vector<std::uint8_t> buffer_;
};

}  // namespace net
