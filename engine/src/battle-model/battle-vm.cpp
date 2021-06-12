// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./battle-vm.h"
#include "battle-view/shaders.h"


int BattleVM::Loop::findLoop(const std::vector<Loop>& loops, BattleVM::LoopType type) {
  const auto count = std::count_if(loops.begin(), loops.end(), [type](const auto& loop) {
    return loop.type == type;
  });
  if (count) {
    auto n = std::rand() % count;
    for (auto i = loops.begin(); i != loops.end(); ++i) {
      if (i->type == type) {
        if (n) {
          --n;
        } else {
          return i - loops.begin();
        }
      }
    }
    LOG_ASSERT(false);
  }
  if (type != LoopType::None) {
    return findLoop(loops, LoopType::None);
  }
  return 0;
}


const BattleVM::MissileStats* BattleVM::Unit::FindMissileStats(int missileType) const {
  for (const auto& weapon : weapons) {
    for (const auto& missileStats : weapon.missileStats) {
      if (missileStats.id == missileType) {
        return &missileStats;
      }
    }
  }
  return nullptr;
}


float BattleVM::Unit::GetRoutingBlinkTime() const {
  float morale = object ? object["_effectiveMorale"_float] : -1;
  return 0 <= morale && morale < 0.33f ? 0.1f + morale * 3 : 0;
}


void BattleVM::Unit::Animate(float seconds) {
  if (object) {
    float routingBlinkTime = GetRoutingBlinkTime();
    if (!object["_routing"_bool] && routingBlinkTime != 0) {
      routingTimer -= seconds;
      if (routingTimer < 0)
        routingTimer = routingBlinkTime;
    }
  }
}


WeakPtr<BattleVM::Unit> BattleVM::Model::FindUnit(ObjectId unitId) const {
  for (const auto& unit : units) {
    if (unit->unitId == unitId) {
      return unit;
    }
  }
  return {};
}


const std::vector<RootPtr<BattleVM::Shape>>& BattleVM::Model::GetShapes(const std::string& name) const {
  auto i = shapes.find(name);
  if (i != shapes.end()) {
    return i->second;
  }
  return defaultShape;
}


BackPtr<BattleVM::Shape> BattleVM::Model::GetShape(const std::string& name) const {
  return GetShapes(name).front();
}
