// Copyright 2014-2015 the openage authors. See copying.md for legal info.

#include <memory>

#include "../terrain/terrain_object.h"
#include "../datamanager.h"
#include "ability.h"
#include "action.h"
#include "command.h"
#include "unit.h"

namespace openage {

bool has_hitpoints(Unit &target) {
	return target.has_attribute(attr_type::hitpoints) &&
	       target.get_attribute<attr_type::hitpoints>().current > 0;
}

bool has_resource(Unit &target) {
	return target.has_attribute(attr_type::resource) &&
	       target.get_attribute<attr_type::resource>().amount > 0;
}

bool is_enemy(Unit &to_modify, Unit &target) {
	if (to_modify.has_attribute(attr_type::owner) &&
		target.has_attribute(attr_type::owner)) {
		auto &mod_player = to_modify.get_attribute<attr_type::owner>().player;
		auto &tar_player = target.get_attribute<attr_type::owner>().player;
		return mod_player.is_enemy(tar_player);
	}
	return false;
}

MoveAbility::MoveAbility(Sound *s)
	:
	sound{s} {
}

bool MoveAbility::can_invoke(Unit &to_modify, const Command &cmd) {
	if (cmd.has_position()) {
		return to_modify.location;
	}
	else if (cmd.has_unit()) {
		return to_modify.location &&
		       &to_modify != cmd.unit(); // cannot target self
	}
	return false; 
}

void MoveAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke move action");
	if (play_sound && this->sound) {
		this->sound->play();
	}

	if (cmd.has_position()) {
		auto target = cmd.position();
		to_modify.push_action(std::make_unique<MoveAction>(&to_modify, target));
	}
	else if (cmd.has_unit()) {
		auto target = cmd.unit();

		// distance from the targets edge that is required to stop moving
		coord::phys_t radius = path::path_grid_size + (to_modify.location->min_axis() / 2);
		if (to_modify.has_attribute(attr_type::attack)) {
			auto &att = to_modify.get_attribute<attr_type::attack>();
			radius += att.range;
		}
		if (target->has_attribute(attr_type::speed)) {
			auto &sp = target->get_attribute<attr_type::speed>();
			radius += 8 * sp.unit_speed;
		}

		to_modify.push_action(std::make_unique<MoveAction>(&to_modify, target->get_ref(), radius));
	}
}

GarrisonAbility::GarrisonAbility(Sound *s)
	:
	sound{s} {
}

bool GarrisonAbility::can_invoke(Unit &to_modify, const Command &cmd) {
	return to_modify.location &&
	       cmd.has_unit() &&
	       cmd.unit()->has_attribute(attr_type::garrison);
}

void GarrisonAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke garrison action");
	if (play_sound && this->sound) {
		this->sound->play();
	}
	to_modify.push_action(std::make_unique<GarrisonAction>(&to_modify, cmd.unit()->get_ref()));
}

UngarrisonAbility::UngarrisonAbility(Sound *s)
	:
	sound{s} {
}

bool UngarrisonAbility::can_invoke(Unit &, const Command &cmd) {
	return cmd.has_position();
}

void UngarrisonAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke ungarrison action");
	if (play_sound && this->sound) {
		this->sound->play();
	}
	to_modify.push_action(std::make_unique<UngarrisonAction>(&to_modify, cmd.position()));
}

TrainAbility::TrainAbility(Sound *s)
	:
	sound{s} {
}

bool TrainAbility::can_invoke(Unit &, const Command &cmd) {
	return cmd.has_producer();
}

void TrainAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke train action");
	if (play_sound && this->sound) {
		this->sound->play();
	}
	to_modify.push_action(std::make_unique<TrainAction>(&to_modify, cmd.producer()));
}

BuildAbility::BuildAbility(Sound *s)
	:
	sound{s} {
}

bool BuildAbility::can_invoke(Unit &, const Command &cmd) {
	if (cmd.has_producer() && cmd.has_position()) {
		return true;
	}
	if (cmd.has_unit()) {
		Unit *target = cmd.unit();
		return target->has_attribute(attr_type::building) &&
		       target->get_attribute<attr_type::building>().completed < 1.0f;
	}
	return false;
}

void BuildAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke build action");
	if (play_sound && this->sound) {
		this->sound->play();
	}

	if (cmd.has_unit()) {
		to_modify.push_action(std::make_unique<BuildAction>(&to_modify, cmd.unit()->get_ref()));
	}
	else {
		to_modify.push_action(std::make_unique<BuildAction>(&to_modify, cmd.producer(), cmd.position()));
	}
}

GatherAbility::GatherAbility(Sound *s)
	:
	sound{s} {
}

bool GatherAbility::can_invoke(Unit &to_modify, const Command &cmd) {
	if (cmd.has_unit()) {
		Unit &target = *cmd.unit();
		return &to_modify != &target &&
		       to_modify.has_attribute(attr_type::gatherer) &&
		       has_resource(target);
	}
	return false;
}

void GatherAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke gather action");
	if (play_sound && this->sound) {
		this->sound->play();
	}

	Unit *target = cmd.unit();
	to_modify.push_action(std::make_unique<GatherAction>(&to_modify, target->get_ref()));
}

AttackAbility::AttackAbility(Sound *s)
	:
	sound{s} {
}

bool AttackAbility::can_invoke(Unit &to_modify, const Command &cmd) {
	if (cmd.has_unit()) {
		Unit &target = *cmd.unit();
		return &to_modify != &target &&
		       to_modify.has_attribute(attr_type::attack) &&
		       has_hitpoints(target) &&
		       (is_enemy(to_modify, target) || has_resource(target));
	}
	return false;
}

void AttackAbility::invoke(Unit &to_modify, const Command &cmd, bool play_sound) {
	to_modify.log(MSG(dbg) << "invoke attack action");
	if (play_sound && this->sound) {
		this->sound->play();
	}

	Unit *target = cmd.unit();
	to_modify.push_action(std::make_unique<AttackAction>(&to_modify, target->get_ref()));
}

} /* namespace openage */

namespace std {

string to_string(const openage::ability_type &at) {
	switch (at) {
	case openage::ability_type::move:
		return "move";
	case openage::ability_type::garrison:
		return "garrison";
	case openage::ability_type::ungarrison:
		return "ungarrison";
	case openage::ability_type::patrol:
		return "patrol";
	case openage::ability_type::train:
		return "train";
	case openage::ability_type::build:
		return "build";
	case openage::ability_type::research:
		return "research";
	case openage::ability_type::gather:
		return "gather";
	case openage::ability_type::attack:
		return "attack";
	case openage::ability_type::convert:
		return "convert";
	case openage::ability_type::repair:
		return "repair";
	case openage::ability_type::heal:
		return "heal";
	}
	return "unknown";
}	

} // namespace std
