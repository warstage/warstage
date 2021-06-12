// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_SIMULATOR_CONVERT_VALUE_H
#define WARSTAGE__BATTLE_SIMULATOR_CONVERT_VALUE_H

#include "./battle-objects.h"
#include "runtime/runtime.h"
#include "battle-model/battle-sm.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>


std::vector<glm::vec2> DecodeArrayVec2(const Value& value);
std::array<float, 25> DecodeRangeValues(const Value& value);

Value ProjectileToBson(const std::vector<BattleSM::Projectile>& value);
std::vector<BattleSM::Projectile> ProjectileFromBson(const Value& value);

Value FormationToBson(const BattleSM::Formation& value);
BattleSM::Formation FormationFromBson(const Value& value);


#endif
