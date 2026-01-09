#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace dungeon {

struct MovementSample {
    std::uint64_t character_id{0};
    float distance{0.0f};
    std::chrono::milliseconds elapsed{0};
};

class MovementValidator {
public:
    explicit MovementValidator(float max_speed);

    bool validate(const MovementSample &sample, std::string &reason) const;

private:
    float max_speed_;
};

}  // namespace dungeon
