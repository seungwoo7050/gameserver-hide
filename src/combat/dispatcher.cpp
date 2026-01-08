#include "combat/dispatcher.h"

namespace combat {

void Dispatcher::setSkillHandler(SkillHandler handler) {
    skill_handler_ = std::move(handler);
}

void Dispatcher::setDamageHandler(DamageHandler handler) {
    damage_handler_ = std::move(handler);
}

DamageEvent Dispatcher::processSkillEvent(const SkillEvent &event) {
    std::optional<DamageEvent> derived;
    if (skill_handler_) {
        derived = skill_handler_(event);
    }

    DamageEvent damage_event = derived.value_or(buildDamageFromSkill(event));
    processDamageEvent(damage_event);
    return damage_event;
}

void Dispatcher::processDamageEvent(const DamageEvent &event) {
    damage_history_.push_back(event);
    if (damage_handler_) {
        damage_handler_(event);
    }
}

const std::vector<DamageEvent> &Dispatcher::damageHistory() const {
    return damage_history_;
}

DamageEvent Dispatcher::buildDamageFromSkill(const SkillEvent &event) const {
    DamageEvent damage;
    damage.source_id = event.attacker_id;
    damage.target_id = event.target_id;
    damage.skill_id = event.skill_id;
    damage.amount = event.base_damage;
    return damage;
}

}  // namespace combat
