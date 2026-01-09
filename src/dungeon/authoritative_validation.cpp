#include "dungeon/authoritative_validation.h"

namespace dungeon {

MovementValidator::MovementValidator(float max_speed) : max_speed_(max_speed) {}

bool MovementValidator::validate(const MovementSample &sample, std::string &reason) const {
    if (sample.elapsed.count() <= 0) {
        reason = "Invalid elapsed time";
        return false;
    }
    float seconds = static_cast<float>(sample.elapsed.count()) / 1000.0f;
    float speed = sample.distance / seconds;
    if (speed > max_speed_) {
        reason = "Movement speed exceeds server limit";
        return false;
    }
    return true;
}

}  // namespace dungeon
