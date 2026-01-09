#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace combat {

using EntityId = std::uint64_t;

struct SkillEvent {
    EntityId attacker_id{0};
    EntityId target_id{0};
    std::uint32_t skill_id{0};
    std::int32_t base_damage{0};
};

struct DamageEvent {
    EntityId source_id{0};
    EntityId target_id{0};
    std::uint32_t skill_id{0};
    std::int32_t amount{0};
};

using SkillValidator = std::function<bool(const SkillEvent &event)>;

class Dispatcher {
public:
    using SkillHandler = std::function<std::optional<DamageEvent>(const SkillEvent &event)>;
    using DamageHandler = std::function<void(const DamageEvent &event)>;

    void setSkillValidator(SkillValidator validator);
    void setSkillHandler(SkillHandler handler);
    void setDamageHandler(DamageHandler handler);

    DamageEvent processSkillEvent(const SkillEvent &event);
    void processDamageEvent(const DamageEvent &event);

    const std::vector<DamageEvent> &damageHistory() const;

private:
    DamageEvent buildDamageFromSkill(const SkillEvent &event) const;

    SkillValidator skill_validator_{};
    SkillHandler skill_handler_;
    DamageHandler damage_handler_;
    std::vector<DamageEvent> damage_history_;
};

}  // namespace combat
