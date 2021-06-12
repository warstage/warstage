// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./battle-simulator.h"
#include "./convert-value.h"
#include <cstdlib>
#include <glm/gtc/random.hpp>
#include <sstream>

using namespace BattleSM;


bool BattleModel::IsInMelee(Unit& unit) {
  int count = 0;
  for (const auto& element : unit.elements) {
    if (element->state.melee.opponent && ++count >= 3) {
      return true;
    }
  }
  return false;
}


glm::vec2 BattleModel::CalculateUnitCenter(const Unit& unit) {
  if (unit.state.formation.unitMode == UnitMode::Initializing)
    return unit.state.formation.center;

  if (unit.elements.empty())
    return unit.state.formation.center;

  glm::vec2 p = glm::vec2();
  int count = 0;

  for (const auto& element : unit.elements) {
    p += element->state.body.position;
    ++count;
  }

  return p / (float)count;
}


float BattleModel::GetCurrentSpeed(const Unit& unit) {
  for (const auto& subunit : unit.stats.subunits) {
    if (unit.state.emotion.IsRouting()) {
      return subunit.stats.movement.routingSpeed;
    } else {
      return unit.command.running || unit.command.meleeTarget ? subunit.stats.movement.runningSpeed : subunit.stats.movement.walkingSpeed;
    }
  }
  return 0.0f;
}


Element* BattleModel::GetElement(const Unit& unit, int rank, int file) {
  if (0 <= rank && rank < unit.formation.numberOfRanks && file >= 0) {
    std::size_t index = rank + file * unit.formation.numberOfRanks;
    if (index < unit.elements.size()) {
      return unit.elements[index].get();
    }
  }
  return nullptr;
}


glm::vec2 BattleModel::GetFrontLeft(const Formation& formation, glm::vec2 center) {
  return center
      - formation.towardRight * (0.5f * static_cast<float>(formation.numberOfFiles - 1))
      - formation.towardBack * (0.5f * static_cast<float>(formation.numberOfRanks - 1));
}


BattleSimulator::BattleSimulator(Runtime& runtime) {
  simulatorStrand_ = Strand::makeStrand("simulator");
  battleFederate_ = std::make_shared<Federate>(runtime, "Battle/Simulator", simulatorStrand_);
  model_ = std::make_unique<BattleModel>();
}


void BattleSimulator::Startup(ObjectId battleFederationId) {
  auto weak_ = weak_from_this();
  simulatorStrand_->setImmediate([weak_, battleFederationId]() {
    if (auto this_ = weak_.lock())
      this_->Initialize(battleFederationId);
  });
}


Promise<void> BattleSimulator::shutdown_() {
  co_await *simulatorStrand_;

  if (interval_) {
      acquireTerrainMap();
    clearInterval(*interval_);
    interval_ = nullptr;
      releaseTerrainMap();
  }

  co_await battleFederate_->shutdown();

    acquireTerrainMap();
  battleFederate_ = nullptr;
    releaseTerrainMap();

  unitLookup_.clear();
  model_->fighterQuadTree.clear();
  model_->weaponQuadTree.clear();
  model_->units.clear();
}


void BattleSimulator::Initialize(ObjectId battleFederationId) {
  if (battleFederate_->shutdownStarted()) {
    return LOG_W("BattleSimulator::Initialize: federate is shutdown");
  }

  auto weak_ = weak_from_this();

  battleFederate_->getObjectClass("Terrain").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      if (object.justDiscovered()) {
        this_->terrain_ = object;
      } else if (object.justDestroyed()) {
        this_->terrain_ = {};
      }
    }
  });

  battleFederate_->getObjectClass("Unit").require({
      "commander", "alliance", "unitType", "stats.placement"
  });
  battleFederate_->getObjectClass("Unit").publish({
      "path", "facing", "running", "meleeTarget", "missileTarget", "intrinsicMorale", "fighters"
  });

  battleFederate_->getObjectClass("Unit").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock())
      this_->UnitChanged(object);
  });

  /*battleFederate_->getEventClass("_SetMap").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock()) {
    }
  });*/

  battleFederate_->getEventClass("ControlDeployUnit").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock()) {
        this_->acquireTerrainMap();
      if (this_->battleFederate_) {
        this_->DeployUnit(
            event["unit"_ObjectId],
            event["position"_vec2],
            event["bearing"_float]
        );
      }
        this_->releaseTerrainMap();
    }
  });

  battleFederate_->getEventClass("Command").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock()) {
      float latency = this_->battleFederate_->getEventLatency();
      this_->ProcessCommandEvent(event, latency);
    }
  });

  battleFederate_->getEventClass("_Commander").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock()) {
      this_->ProcessCommanderEvent(event);
    }
  });

  battleFederate_->getEventClass("MissileRelease").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock()) {
      if (auto unit = this_->FindUnit(event["unit"_ObjectId])) {
        int missileType = event["missileType"_int];
        if (const auto& missileStats = unit->FindMissileStats(missileType)) {
          float delay = this_->battleFederate_->getEventDelay();
          float latency = this_->battleFederate_->getEventLatency();
          Shooting shooting{};
          shooting.unitId = unit->object.getObjectId();
          shooting.missileType = missileType;
          shooting.maximumRange = missileStats->maximumRange;
          shooting.hitRadius = event["hitRadius"_float];
          shooting.projectiles = ProjectileFromBson(event["projectiles"]);
          shooting.timeToImpact = event["timeToImpact"_float] - latency;
          this_->shootings_.emplace_back(delay - latency, shooting);
        }
      } else {
        LOG_ASSERT(unit);
      }
    }
  });

  battleFederate_->startup(battleFederationId);

  simulatorStrand_->setImmediate([weak_]() {
    if (auto this_ = weak_.lock()) {
      if (this_->battleFederate_->shutdownStarted()) {
        return LOG_W("BattleSimulator::Initialize: federate is shutdown (2)");
      }
      this_->battleStatistics_ = this_->battleFederate_->getObjectClass("_BattleStatistics").create();
    }
  });

  interval_ = simulatorStrand_->setInterval([weak_]() {
    if (auto this_ = weak_.lock()) {
      if (this_->battleFederate_->shutdownStarted()) {
        return LOG_W("BattleSimulator::Initialize: federate is shutdown (3)");
      }
      this_->SimulateTimeStep();
    }
  }, 1000.0 * timeStep_);
}


void BattleSimulator::acquireTerrainMap() {
  if (terrain_) {
    terrainMap_ = terrain_.acquireShared<TerrainMap*>();
  }
}


void BattleSimulator::releaseTerrainMap() {
  if (terrain_) {
    terrain_.releaseShared();
    terrainMap_ = nullptr;
  }
}


WeakPtr<Unit> BattleSimulator::FindUnit(ObjectId unitId) const {
  auto i = unitLookup_.find(unitId);
  return i == unitLookup_.end() ? WeakPtr<Unit>{} : WeakPtr<Unit>{i->second};
}


void BattleSimulator::UnitChanged(const ObjectRef& object) {
  if (object.justDestroyed()) {
    RemoveUnit(object.getObjectId());
  } else if (object.justDiscovered()) {
      acquireTerrainMap();
    if (battleFederate_) {
      DiscoverUnit(object);
    }
      releaseTerrainMap();
  }
}

static void UpdateUnitFormation(Formation& formation, FormationStats stats, int count) {
  switch (stats.type) {
    case FormationType::Line:
      formation.numberOfRanks = bounds1i{1, stats.ranks}.clamp(count);
      formation.numberOfFiles = (int)ceilf((float)count / formation.numberOfRanks);
      break;
    case FormationType::Column:
      formation.numberOfFiles = bounds1i{1, stats.files}.clamp(count);
      formation.numberOfRanks= (int)ceilf((float)count / formation.numberOfFiles);
      break;
    default:
      formation.numberOfRanks = static_cast<int>(std::sqrt(count));
      formation.numberOfFiles = (int)ceilf((float)count / formation.numberOfRanks);
      break;
  }
}

void BattleSimulator::DiscoverUnit(const ObjectRef& unitObject) {
  model_->units.push_back(MakeUnit(unitObject));
  auto& unit = model_->units.back();

  unitLookup_.emplace(unit->unitId, unit);

  MovementRules_AdvanceTime(*unit, 0);
  unit->nextState = NextUnitState(*unit);
  for (auto i = unit->elements.begin(); i != unit->elements.end(); ++i) {
    (*i)->nextState = NextElementState(**i, i - unit->elements.begin());
  }

  unit->state = unit->nextState;
  for (auto& element : unit->elements) {
    element->state = element->nextState;
  }

  UpdateUnitObjectFromEntity_Local(*unit);
  UpdateUnitObjectFromEntity_Remote(*unit);
}


RootPtr<Unit> BattleSimulator::MakeUnit(const ObjectRef& unitObject) {
  BattleSM::UnitStats stats{};

  if (auto unitType = unitObject["unitType"_value]; unitType.is_document()) {
    stats.training = unitType["training"_float];

    for (const auto& formation : unitType["formations"_value]) {
      stats.formation.type = FormationType::Line;
      stats.formation.ranks = formation["ranks"_int];
      stats.formation.spacing.x = formation["spacing"]["0"_float];
      stats.formation.spacing.y = formation["spacing"]["1"_float];
    }

    for (const auto& subunitDM : unitType["subunits"_value]) {
      auto& subunitSM = stats.subunits.emplace_back();
      subunitSM.individuals = subunitDM["individuals"_int];

      auto element = subunitDM["element"_value];
      subunitSM.stats.body.size.x = element["size"]["0"_float];
      subunitSM.stats.body.size.y = element["size"]["2"_float];
      subunitSM.stats.movement.walkingSpeed = element["movement"]["speed"]["normal"_float];
      subunitSM.stats.movement.runningSpeed = element["movement"]["speed"]["fast"_float];
      subunitSM.stats.movement.routingSpeed = subunitSM.stats.movement.runningSpeed * 1.125f;

      for (const auto& weapon : subunitDM["weapons"_value]) {
        if (auto melee = weapon["melee"_value]; melee.is_document()) {
          subunitSM.weapon.melee.weaponReach = melee["reach"_float];
          subunitSM.weapon.melee.readyingDuration = melee["time"]["ready"_float];
          subunitSM.weapon.melee.strikingDuration = melee["time"]["strike"_float];
        }
        for (const auto& missile : weapon["missiles"_value]) {
          subunitSM.weapon.missile.id = missile["id"_int];
          subunitSM.weapon.missile.minimumRange = missile["range"]["0"_float];
          subunitSM.weapon.missile.maximumRange = missile["range"]["1"_float];
          subunitSM.weapon.missile.missileSpeed = missile["initialSpeed"_float];
          subunitSM.weapon.missile.flatTrajectory = missile["initialSpeed"_float] >= 500.0f;
          subunitSM.weapon.missile.loadingTime = missile["time"]["aim"_float] + missile["time"]["reload"_float];
          subunitSM.weapon.missile.missileDelay = missile["time"]["release"_float];
          subunitSM.weapon.missile.hitRadius = missile["hitRadius"_float];
        }
      }
    }
  }

  auto unit = RootPtr<Unit>{new Unit{}};

  unit->object = unitObject;

  float minimumRange = 0.0f;
  float maximumRange = 0.0f;
  for (const auto& subunit : stats.subunits) {
    if (subunit.weapon.missile.maximumRange > maximumRange) {
      minimumRange = subunit.weapon.missile.minimumRange;
      maximumRange = subunit.weapon.missile.maximumRange;
    }
  }

  if (unit->object["stats.isCavalry"].canSetValue())
    unit->object["stats.isCavalry"] = stats.subunits.front().stats.movement.propulsion == PropulsionMode::Quadruped;
  if (unit->object["stats.isMissile"].canSetValue())
    unit->object["stats.isMissile"] = maximumRange > 0;
  if (unit->object["stats.maximumReach"].canSetValue())
    unit->object["stats.maximumReach"] = stats.subunits.front().weapon.melee.weaponReach;
  if (unit->object["stats.minimumRange"].canSetValue())
    unit->object["stats.minimumRange"] = minimumRange;
  if (unit->object["stats.maximumRange"].canSetValue())
    unit->object["stats.maximumRange"] = maximumRange;


  auto placement = unitObject["stats.placement"_vec3];
  bool canRally = !unitObject["stats.canNotRally"_bool];

  unit->unitId = unitObject.getObjectId();
  unit->allianceId = unitObject["alliance"_ObjectId];
  unit->stats = stats;
  unit->unbuffered.canRally = canRally;

  unit->elements.resize(stats.subunits.front().individuals);
  for (auto& element : unit->elements) {
    element = RootPtr<Element>{new Element{unit}};
  }

  unit->command.facing = placement.z;

  unit->state.formation.unitMode = UnitMode::Initializing;
  unit->state.formation.center = placement.xy();
  unit->state.formation.waypoint = placement.xy();
  unit->state.formation.bearing = placement.z;

  unit->formation.rankDistance = stats.subunits.front().stats.body.size.y + stats.formation.spacing.y;
  unit->formation.fileDistance = stats.subunits.front().stats.body.size.x + stats.formation.spacing.x;

  UpdateUnitFormation(unit->formation, stats.formation, unit->elements.size());

  return unit;
}


void BattleSimulator::DeployUnit(ObjectId unitId, glm::vec2 position, float bearing) {
  if (auto unit = FindUnit(unitId)) {
    unit->state.formation.center = position;
    unit->state.formation.bearing = bearing;

    unit->command = CommandState();
    unit->command.facing = bearing;

    unit->state.formation.unitMode = UnitMode::Initializing;
    MovementRules_AdvanceTime(*unit, 0);
    unit->nextState = NextUnitState(*unit);
    for (auto i = unit->elements.begin(); i != unit->elements.end(); ++i) {
      (*i)->nextState = NextElementState(**i, i - unit->elements.begin());
    }

    unit->state = unit->nextState;
    for (auto& element : unit->elements) {
      element->state = element->nextState;
    }
  }
}


void BattleSimulator::RemoveUnit(ObjectId unitId) {
  if (auto unit = FindUnit(unitId)) {
    //_battle_simulatorThread->Events("UnitRemoved").Dispatch(document{}
    //    << "unit" << unit->unitId
    //    << ValueEnd{});

    for (const auto& other : model_->units) {
      for (auto& element : other->elements) {
        if (element->state.melee.opponent && element->state.melee.opponent->unit == unit)
          element->state.melee.opponent = nullptr;
        if (element->state.melee.target && element->state.melee.target->unit == unit)
          element->state.melee.target = nullptr;
      }

      if (other->command.meleeTarget == unit)
        other->command.meleeTarget = nullptr;
      if (other->command.missileTarget == unit)
        other->command.missileTarget = nullptr;
      if (other->missileTarget == unit)
        other->missileTarget = nullptr;
    }

    unitLookup_.erase(unit->unitId);

    model_->units.erase(
        std::remove_if(model_->units.begin(), model_->units.end(), [&unit](const auto &x) { return x == unit; }),
        model_->units.end());
  }
}


void BattleSimulator::ProcessCommandEvent(const Value& event, float latency) {
  if (auto unit = FindUnit(event["unit"_ObjectId])) {
    double delay = std::max(0.0, commandDelay_ - latency);
    auto object = unit->object;

    if (event["path"].is_defined() && object["path"].canSetValue()) {
      unit->object["path"].setValue(event["path"_value], delay);
      unit->remoteUpdateCountdown = 0;
    }

    if (event["facing"].is_defined() && object["facing"].canSetValue()) {
      unit->object["facing"].setValue(event["facing"_value], delay);
      unit->remoteUpdateCountdown = 0;
    }

    if (event["running"].is_defined() && object["running"].canSetValue()) {
      unit->object["running"].setValue(event["running"_value], delay);
      unit->remoteUpdateCountdown = 0;
    }

    if (event["meleeTarget"].is_defined() && object["meleeTarget"].canSetValue()) {
      unit->object["meleeTarget"].setValue(event["meleeTarget"_value], delay);
      unit->remoteUpdateCountdown = 0;
    }

    if (event["missileTarget"].is_defined() && object["missileTarget"].canSetValue()) {
      unit->object["missileTarget"].setValue(event["missileTarget"_value], delay);
      unit->remoteUpdateCountdown = 0;
    }
  }
}


void BattleSimulator::ProcessCommanderEvent(const Value& event) {
  commanderPlayerId_ = event["playerId"_c_str];
}


/***/


void BattleSimulator::SimulateTimeStep() {
    acquireTerrainMap();
  if (battleFederate_) {
    battleFederate_->updateCurrentTime_strand();
    if (interval_) {
      UpdateUnitEntityFromObject();

      // RebuildQuadTree
      model_->fighterQuadTree.clear();
      model_->weaponQuadTree.clear();

      for (const auto& unit : model_->units) {
        if (unit->state.formation.unitMode != UnitMode::Initializing) {
          for (auto& element : unit->elements) {
            model_->fighterQuadTree.insert(element->state.body.position.x, element->state.body.position.y, &element);

            if (unit->stats.subunits.front().weapon.melee.weaponReach > 0) {
              auto d = unit->stats.subunits.front().weapon.melee.weaponReach * vector2_from_angle(element->state.body.bearing);
              auto p = element->state.body.position + d;
              model_->weaponQuadTree.insert(p.x, p.y, &element);
            }
          }
        }
      }

      // UpdateUnitMovement
      for (const auto& unit : model_->units) {
        MovementRules_AdvanceTime(*unit, timeStep_);
      }

      // ComputeNextState
      for (const auto& unit : model_->units) {
        unit->nextState = NextUnitState(*unit);

        for (auto i = unit->elements.begin(); i != unit->elements.end(); ++i) {
          (*i)->nextState = NextElementState(**i, i - unit->elements.begin());
        }
      }

      // AssignNextState
      for (const auto& unit : model_->units) {
        unit->state = unit->nextState;
        if (unit->state.emotion.IsRouting()) {
          unit->command.path.clear();
          unit->command.path.push_back(unit->state.formation.center);
          unit->command.meleeTarget = nullptr;
        }
        for (auto& element : unit->elements) {
          element->state = element->nextState;
        }
        UpdateUnitRange(*unit);
      }

      // ResolveMeleeCombat
      for (const auto& unit : model_->units) {
        bool isMissile = unit->stats.subunits.front().weapon.missile.maximumRange != 0.0f;
        for (auto& element : unit->elements) {
          auto meleeTarget = element->state.melee.target;
          if (meleeTarget && meleeTarget->unit->object["fighters"].canSetValue()) {
            auto enemyUnit = meleeTarget->unit.get();
            float killProbability = 0.5f;

            killProbability *= 1.25f + unit->stats.training;
            killProbability *= 1.25f - enemyUnit->stats.training;

            if (isMissile) {
              killProbability *= 0.15;
            }

            float heightDiff = element->state.body.position_z - meleeTarget->state.body.position_z;
            killProbability *= 1.0f + 0.4f * bounds1d(-1.5f, 1.5f).clamp(heightDiff);

            float speed = glm::length(element->state.body.velocity);
            killProbability *= (0.9f + speed / 10.0f);

            float roll = (rand() & 0x7FFF) / (float)0x7FFF;

            if (roll < killProbability) {
              meleeTarget->casualty = true;
            } else {
              meleeTarget->state.melee.readyState = ReadyState::Stunned;
              meleeTarget->state.melee.stunnedTimer = 0.6f;
            }

            element->state.melee.readyingTimer = element->unit->stats.subunits.front().weapon.melee.readyingDuration;
          }
        }
      }

      // ResolveMissileCombat
      for (const auto& unit : model_->units) {
        if (unit->object["fighters"].canSetValue() && unit->state.missile.shootingCounter > unit->unbuffered.shootingCounter) {
          TriggerShooting(*unit);
          unit->unbuffered.shootingCounter = unit->state.missile.shootingCounter;
        }
      }

      // ResolveProjectileCasualties
      int random = 0;
      for (auto& s : shootings_) {
        if (s.first > 0) {
          s.first -= timeStep_;
        }
        if (s.first <= 0) {
          auto& shooting = s.second;
          if (!shooting.released) {
            shooting.released = true;
            if (shooting.original) {
              battleFederate_->getEventClass("MissileRelease").dispatch(Struct{}
                      << "unit" << shooting.unitId
                      << "missileType" << shooting.missileType
                      << "hitRadius" << shooting.hitRadius
                      << "timeToImpact" << shooting.timeToImpact
                      << "projectiles" << ProjectileToBson(shooting.projectiles)
                      << ValueEnd{},
                  timerDelay_);
            }
          }
          const auto unit = FindUnit(shooting.unitId);
          if (!unit) {
            continue;
          }
          const auto* missileStats = unit->FindMissileStats(shooting.missileType);
          const bool flatTrajectory = missileStats->flatTrajectory;
          bool largeHitRadius = missileStats->hitRadius >= 2.0f;

          shooting.timeToImpact -= timeStep_;
          auto i = shooting.projectiles.begin();
          while (i != shooting.projectiles.end()) {
            auto& projectile = *i;
            if (shooting.timeToImpact + projectile.delay > 0) {
              ++i;
            } else {
              float radius = shooting.hitRadius;
              if (radius >= 0.0f) {
                auto delta = projectile.position2 - projectile.position1;
                float distance = glm::length(delta);
                float killzone = flatTrajectory ? glm::min(60.0f, distance) : 0.0f;
                float killstep = 4.0f;
                auto hitpoint = projectile.position2;
                if (killzone >= 30.0f) {
                  delta /= distance;
                  hitpoint -= (killzone - 30.0f) * delta;
                  delta *= killstep;
                } else {
                  killzone = 0.0f;
                }

                float hitradius = radius;
                float shrinkage = 0;
                if (flatTrajectory) {
                  float range = shooting.maximumRange;
                  float steps = killzone / killstep;
                  float factor1 = glm::clamp(1.0f - 0.6f * glm::distance(projectile.position1, hitpoint) / range, 0.1f, 1.0f);
                  float factor2 = glm::clamp(1.0f - 0.6f * glm::distance(projectile.position1, hitpoint + delta * steps) / range, 0.1f, 1.0f);
                  hitradius = radius * factor1;
                  shrinkage = radius * (factor2 - factor1) / steps;
                }
                while (killzone >= 0.0f) {
                  for (auto j = model_->fighterQuadTree.find(hitpoint.x, hitpoint.y, hitradius); *j; ++j) {
                    auto& element = ***j;
                    if (element->unit->object["fighters"].canSetValue()) {
                      bool blocked = false;
                      if (largeHitRadius && !element->terrain.forest) {
                        blocked = (random++ & 1) != 0;
                      } else if (!largeHitRadius && element->terrain.forest) {
                        blocked = (random++ & 7) <= 5;
                      }
                      if (!blocked) {
                        element->casualty = true;
                      }
                    }
                    if (!largeHitRadius) {
                      killzone = 0.0f;
                      break;
                    }
                  }
                  killzone -= killstep;
                  hitpoint += delta;
                  hitradius += shrinkage;
                }
                shooting.projectiles.erase(i);
              }
            }
            ++random;
          }
        }
      }

      // RemoveCasualties
      if (terrainMap_) {
        auto bounds = terrainMap_->getHeightMap().getBounds();
        auto center = bounds.mid();
        float radius = bounds.x().size() / 2;
        float radius_squared = radius * radius;
        for (const auto& unit : model_->units) {
          if (unit->object["fighters"].canSetValue()) {
            for (auto& element : unit->elements) {
              if (!element->casualty) {
                element->casualty = element->terrain.impassable && unit->state.emotion.IsRouting();
              }
              if (!element->casualty) {
                auto diff = element->state.body.position - center;
                element->casualty = glm::dot(diff, diff) >= radius_squared;
              }
            }
          }
        }
        for (const auto& unit : model_->units) {
          for (auto& element : unit->elements) {
            if (element->state.melee.opponent && element->state.melee.opponent->casualty) {
              element->state.melee.opponent = nullptr;
            }
          }
        }
        for (const auto& unit : model_->units) {
          std::vector<glm::vec2> casualties{};
          auto i = unit->elements.begin();
          while (i != unit->elements.end()) {
            if ((*i)->casualty) {
              ++unit->state.recentCasualties;
              casualties.push_back((*i)->terrain.position);
              i = unit->elements.erase(i);
            } else {
              ++i;
            }
          }
          allianceCasualtyCount_[unit->allianceId] += casualties.size();
          for (const auto& casualty : casualties) {
            battleFederate_->getEventClass("FighterCasualty").dispatch(Struct{}
                << "unit" << unit->unitId
                << "fighterCount" << static_cast<int>(unit->elements.size())
                << "fighter" << casualty
                << ValueEnd{});
          }
        }
      }

      // UpdateTeamKills
      for (auto& i : allianceCasualtyCount_) {
        auto allianceId = i.first;
        auto teamKills = battleFederate_->getObjectClass("TeamKills").find([allianceId](ObjectRef x) {
          return x["alliance"_ObjectId] == allianceId;
        });
        if (!teamKills) {
          teamKills = battleFederate_->getObjectClass("TeamKills").create();
          teamKills["alliance"] = allianceId;
        }
        if (teamKills["kills"].canSetValue())
          teamKills["kills"] = i.second;
      }

      // RemoveFinishedShootings
      auto i = std::remove_if(shootings_.begin(), shootings_.end(), [](const std::pair<float, Shooting>& s) { return s.second.projectiles.empty(); });
      shootings_.erase(i, shootings_.end());

      // UpdateUnitDeployed
      for (const auto& unit : model_->units) {
        if (!unit->unbuffered.deployed && !unit->elements.empty() && !IsDeploymentZone(unit->allianceId, unit->state.formation.center))
          unit->unbuffered.deployed = true;
      }

      // RouteDefeatedUnits
      for (const auto& alliance : battleFederate_->getObjectClass("Alliance")) {
        if (alliance["defeated"_bool]) {
          auto allianceId = alliance.getObjectId();
          for (const auto& unit : model_->units)
            if (unit->allianceId == allianceId)
              unit->state.emotion.intrinsicMorale = -1.0f;
        }
      }

      UpdateUnitObjectsFromEntities();

      // UpdateBattleStatistics
      if (battleStatistics_) {
        battleStatistics_["countCavalryInMelee"] = model_->CountCavalryInMelee();
        battleStatistics_["countInfantryInMelee"] = model_->CountInfantryInMelee();
      }
    }
  }
    releaseTerrainMap();
}


void BattleSimulator::UpdateUnitEntityFromObject() {
  for (const auto& unit : model_->units) {
    int pathVersion = unit->object["path"].getVersion();
    if (pathVersion != unit->command.pathVersion) {
      unit->command.path = DecodeArrayVec2(unit->object["path"].getValue());
      unit->command.pathVersion = pathVersion;
      if (unit->command.path.size() >= 2) {
        auto center = unit->command.path[0];
        auto delta = center - unit->command.path[1];
        float length = glm::length(delta);
        if (length >= 1) {
          float time = bounds1f{-0.9f, 0.5f}.clamp(static_cast<float>(unit->object["path"].getTime()));
          float speed = unit->command.running ? unit->stats.subunits.front().stats.movement.runningSpeed : unit->stats.subunits.front().stats.movement.walkingSpeed;
          center += delta * (time * speed / length);
        }
        unit->state.formation.center = center;
      } else if (unit->command.path.size() >= 1) {
        unit->state.formation.center = unit->command.path.front();
      }
    }

    int facingVersion = unit->object["facing"].getVersion();
    if (facingVersion != unit->command.facingVersion) {
      unit->command.facing = unit->object["facing"_float];
      unit->command.facingVersion = facingVersion;
    }

    int runningVersion = unit->object["running"].getVersion();
    if (runningVersion != unit->command.runningVersion) {
      unit->command.running = unit->object["running"_bool];
      unit->command.runningVersion = runningVersion;
    }

    int meleeTargetVersion = unit->object["meleeTarget"].getVersion();
    if (meleeTargetVersion != unit->command.meleeTargetVersion) {
      unit->command.meleeTarget = FindUnit(unit->object["meleeTarget"_ObjectId]);
      unit->command.meleeTargetVersion = meleeTargetVersion;
    }

    int missileTargetVersion = unit->object["missileTarget"].getVersion();
    if (missileTargetVersion != unit->command.missileTargetVersion) {
      unit->command.missileTarget = FindUnit(unit->object["missileTarget"_ObjectId]);
      unit->command.missileTargetVersion = missileTargetVersion;
    }

    if (unit->object["intrinsicMorale"].getVersion() != unit->intrinsicMoraleVersion) {
      unit->state.emotion.intrinsicMorale = unit->object["intrinsicMorale"_float];
      unit->intrinsicMoraleVersion = unit->object["intrinsicMorale"].getVersion();
    }

    if (unit->object["fighters"].getVersion() != unit->fightersVersion) {
      glm::vec2 adjust{};
      if (unit->command.path.size() >= 2) {
        auto center = unit->command.path[0];
        auto delta = center - unit->command.path[1];
        float length = glm::length(delta);
        if (length >= 1) {
          float time = bounds1f{-0.9f, 0.5f}.clamp(static_cast<float>(unit->object["fighters"].getTime()));
          float speed = unit->command.running ? unit->stats.subunits.front().stats.movement.runningSpeed : unit->stats.subunits.front().stats.movement.walkingSpeed;
          adjust = delta * (time * speed / length);
        }
      }

      auto elements = DecodeArrayVec2(unit->object["fighters"_value]);
      std::size_t elementCount = static_cast<int>(elements.size());
      if (elementCount < unit->elements.size())
        unit->elements.resize(elementCount);

      auto heightMap = terrainMap_ ? &terrainMap_->getHeightMap() : nullptr;
      for (std::size_t index = 0; index < unit->elements.size(); ++index) {
        auto p = elements[index];
        float h = heightMap ? heightMap->interpolateHeight(p) : 0;
        auto value = glm::vec3{p + adjust, h};
        ElementState& s = unit->elements[index]->state;
        s.body.position = value.xy();
        s.body.position_z = value.z;
        s.melee.readyState = ReadyState::Unready;
        s.melee.readyingTimer = 0;
        s.melee.strikingTimer = 0;
        s.melee.stunnedTimer = 0;
        s.melee.opponent = nullptr;
        unit->elements[index]->casualty = false;
        unit->unbuffered.timeUntilSwapElements = 0.2f;
      }

      unit->fightersVersion = unit->object["fighters"].getVersion();
    }
  }
}


void BattleSimulator::MovementRules_AdvanceTime(Unit& unit, float timeStep) {
  UpdateUnitOrdersPath(unit.command.path, unit.state.formation.center, unit.command.meleeTarget.get());

  UpdateUnitFormation(unit.formation, unit.stats.formation, unit.elements.size());

  float direction = unit.command.facing;

  if (unit.command.path.size() > 1) {
    auto diff = unit.command.path[1] - unit.command.path[0];
    if (glm::length(diff) > 5)
      direction = ::angle(diff);
  }

  if (unit.command.meleeTarget && glm::length(unit.state.formation.center - unit.command.meleeTarget->state.formation.center) <= 15)
    direction = angle(unit.command.meleeTarget->state.formation.center - unit.state.formation.center);

  if (fabsf(direction - unit.formation._direction) > 0.1f) {
    unit.unbuffered.timeUntilSwapElements = 0;
  }

  unit.formation.SetDirection(direction);

  if (unit.unbuffered.timeUntilSwapElements <= timeStep) {
    MovementRules_SwapElements(unit);
    unit.unbuffered.timeUntilSwapElements = 5;
  } else {
    unit.unbuffered.timeUntilSwapElements -= timeStep;
  }
}


struct FighterPos { ElementState state; glm::vec2 pos; };
static bool SortLeftToRight(const FighterPos& v1, const FighterPos& v2) { return v1.pos.y > v2.pos.y; }
static bool SortFrontToBack(const FighterPos& v1, const FighterPos& v2) { return v1.pos.x > v2.pos.x; }


void BattleSimulator::MovementRules_SwapElements(Unit& unit) {
  std::vector<FighterPos> elements;

  float direction = unit.formation._direction;

  for (auto& element : unit.elements) {
    FighterPos fighterPos;
    fighterPos.state = element->state;
    fighterPos.pos = rotate(element->state.body.position, -direction);
    elements.push_back(fighterPos);
  }

  std::sort(elements.begin(), elements.end(), SortLeftToRight);

  std::size_t index = 0;
  while (index < unit.elements.size()) {
    int count = unit.elements.size() - index;
    if (count > unit.formation.numberOfRanks)
      count = unit.formation.numberOfRanks;

    auto begin = elements.begin() + index;
    std::sort(begin, begin + count, SortFrontToBack);
    while (count-- != 0) {
      unit.elements[index]->state = elements[index].state;
      ++index;
    }
  }
}


UnitBufferedState BattleSimulator::NextUnitState(Unit& unit) const {
  if (unit.elements.empty())
    return unit.state;

  UnitBufferedState result;

  result.formation.center = BattleModel::CalculateUnitCenter(unit);
  result.formation.bearing = NextUnitDirection(unit);
  result.formation.unitMode = NextUnitMode(unit);

  result.missile.shootingCounter = unit.state.missile.shootingCounter;

  if (unit.command.missileTarget.get() == &unit) {
    // hold fire
    unit.missileTarget = nullptr;
  } else if (unit.command.missileTarget) {
    // ordered target only
    unit.missileTarget = BattleModel::IsWithinLineOfFire(unit, unit.command.missileTarget->state.formation.center)
        ? unit.command.missileTarget : WeakPtr<Unit>{};
  } else {
    // fire at will - closest target
    if (unit.missileTarget && !BattleModel::IsWithinLineOfFire(unit, unit.missileTarget->state.formation.center)) {
      unit.missileTarget = nullptr;
    }
    if (!unit.missileTarget) {
      unit.missileTarget = model_->ClosestEnemyWithinLineOfFire(unit);
    }
  }

  if (unit.command.running && result.formation.unitMode != UnitMode::Moving) {
    unit.command.running = false;
  }

  if (unit.state.formation.unitMode != UnitMode::Standing || !unit.missileTarget) {
    result.missile.loadingTimer = 0;
    result.missile.loadingDuration = 0;
  } else if (unit.state.missile.loadingTimer + timeStep_ < unit.state.missile.loadingDuration) {
    result.missile.loadingTimer = unit.state.missile.loadingTimer + timeStep_;
    result.missile.loadingDuration = unit.state.missile.loadingDuration;
  } else {
    if (unit.state.missile.loadingDuration > 0 && unit.missileTarget && BattleModel::IsWithinLineOfFire(unit, unit.missileTarget->state.formation.center)) {
      ++result.missile.shootingCounter;
    }

    float loadingTime = 0.0f;
    for (const auto& subunit : unit.stats.subunits) {
      loadingTime = glm::max(loadingTime, subunit.weapon.missile.loadingTime);
    }

    result.missile.loadingTimer = 0;
    result.missile.loadingDuration = loadingTime + (rand() % 100) / 200.0f;
  }

  result.emotion.intrinsicMorale = unit.state.emotion.intrinsicMorale;
  if (unit.state.recentCasualties > 0) {
    result.emotion.intrinsicMorale -= unit.state.recentCasualties * (2.4f - unit.stats.training) / 100;
  } else if (-0.2f < result.emotion.intrinsicMorale && result.emotion.intrinsicMorale < 1) {
    result.emotion.intrinsicMorale += (0.1f + unit.stats.training) / 2000;
  }

  if (result.emotion.intrinsicMorale > -1 && AllianceHasAbandondedBattle(unit.allianceId)) {
    result.emotion.intrinsicMorale -= 1.0f / 250;
  }

  for (const auto& other : model_->units) {
    if (other && other.get() != &unit && other->allianceId == unit.allianceId) {
      float distance = glm::length(other->state.formation.center - unit.state.formation.center);
      float weight = 1.0f * 50.0f / (distance + 50.0f);
      result.emotion.influence -= weight
          * (1 - other->state.emotion.intrinsicMorale)
          * (1 - unit.stats.training)
          * other->stats.training;
    }
  }

  if (unit.state.emotion.IsRouting() && !unit.unbuffered.canRally) {
    result.emotion.intrinsicMorale = -1;
  }

  if (unit.elements.size() <= 8) {
    result.emotion.intrinsicMorale = -1;
  }

  result.formation.waypoint = MovementRules_NextWaypoint(unit);

  return result;
}


float BattleSimulator::NextUnitDirection(Unit& unit) const {
  if (true) // unit.movement
    return unit.command.facing;
  else
    return unit.state.formation.bearing;
}


UnitMode BattleSimulator::NextUnitMode(Unit& unit) const {
  switch (unit.state.formation.unitMode) {
    case UnitMode::Initializing:
      return UnitMode::Standing;

    case UnitMode::Standing:
      if (unit.command.path.size() > 2 || glm::length(unit.state.formation.center - unit.command.GetDestination()) > 8)
        return UnitMode::Moving;
      break;

    case UnitMode::Moving:
      if (unit.command.path.size() <= 2 && glm::length(unit.state.formation.center - unit.command.GetDestination()) <= 8)
        return UnitMode::Standing;
      break;

    default:
      break;
  }
  return unit.state.formation.unitMode;
}


bool BattleSimulator::AllianceHasAbandondedBattle(ObjectId allianceId) const {
  for (const auto& commander : battleFederate_->getObjectClass("Commander"))
    if (commander["alliance"_ObjectId] == allianceId && !commander["abandoned"_bool])
      return false;
  return true;
}


glm::vec2 BattleSimulator::MovementRules_NextWaypoint(Unit& unit) const {
  for (auto p : unit.command.path)
    if (glm::distance(p, unit.state.formation.center) > 1.0f)
      return p;

  if (unit.command.meleeTarget)
    return unit.command.meleeTarget->state.formation.center;

  if (!unit.command.path.empty())
    return unit.command.path.back();

  return unit.state.formation.center;
}


ElementState BattleSimulator::NextElementState(Element& element, int index) const {
  auto& original = element.state;
  ElementState result;

  result.melee.readyState = original.melee.readyState;
  result.body.position = NextElementPosition(element, index);
  result.body.position_z = terrainMap_ ? terrainMap_->getHeightMap().interpolateHeight(result.body.position) : 0.0f;
  result.body.velocity = NextElementVelocity(element);


  // DIRECTION

  if (element.unit->state.formation.unitMode == UnitMode::Moving) {
    result.body.bearing = angle(original.body.velocity);
  } else if (original.melee.opponent) {
    result.body.bearing = angle(original.melee.opponent->state.body.position - original.body.position);
  } else {
    result.body.bearing = element.unit->state.formation.bearing;
  }


  // OPPONENT

  if (original.melee.opponent
      && glm::length(original.body.position - original.melee.opponent->state.body.position) <= element.unit->stats.subunits.front().weapon.melee.weaponReach * 2) {
    result.melee.opponent = original.melee.opponent;
  } else if (element.unit->state.formation.unitMode != UnitMode::Moving && !element.unit->state.emotion.IsRouting()) {
    result.melee.opponent = FindStrikingTarget(element);
  }

  // DESTINATION

  result.body.destination = MovementRules_NextDestination(element, index);

  // READY STATE

  switch (original.melee.readyState) {
    case ReadyState::Unready:
      if (element.unit->command.meleeTarget) {
        result.melee.readyState = ReadyState::Prepared;
      } else if (element.unit->state.formation.unitMode == UnitMode::Standing) {
        result.melee.readyState = ReadyState::Readying;
        result.melee.readyingTimer = element.unit->stats.subunits.front().weapon.melee.readyingDuration;
      }
      break;

    case ReadyState::Readying:
      if (original.melee.readyingTimer > timeStep_) {
        result.melee.readyingTimer = original.melee.readyingTimer - timeStep_;
      } else {
        result.melee.readyingTimer = 0;
        result.melee.readyState = ReadyState::Prepared;
      }
      break;

    case ReadyState::Prepared:
      if (element.unit->state.formation.unitMode == UnitMode::Moving && !element.unit->command.meleeTarget) {
        result.melee.readyState = ReadyState::Unready;
      } else if (result.melee.opponent) {
        result.melee.readyState = ReadyState::Striking;
        result.melee.strikingTimer = element.unit->stats.subunits.front().weapon.melee.strikingDuration;
      }
      break;

    case ReadyState::Striking:
      if (original.melee.strikingTimer > timeStep_) {
        result.melee.strikingTimer = original.melee.strikingTimer - timeStep_;
        result.melee.opponent = original.melee.opponent;
      } else {
        result.melee.target = original.melee.opponent;
        result.melee.strikingTimer = 0;
        result.melee.readyState = ReadyState::Readying;
        result.melee.readyingTimer = element.unit->stats.subunits.front().weapon.melee.readyingDuration;
      }
      break;

    case ReadyState::Stunned:
      if (original.melee.stunnedTimer > timeStep_) {
        result.melee.stunnedTimer = original.melee.stunnedTimer - timeStep_;
      } else {
        result.melee.stunnedTimer = 0;
        result.melee.readyState = ReadyState::Readying;
        result.melee.readyingTimer = element.unit->stats.subunits.front().weapon.melee.readyingDuration;
      }
      break;
  }

  return result;
}


glm::vec2 BattleSimulator::NextElementPosition(Element& element, int index) const {
  auto unit = element.unit.get();

  if (unit->state.formation.unitMode == UnitMode::Initializing) {
    int rank = index % unit->formation.numberOfRanks;
    int file = index / unit->formation.numberOfRanks;

    auto center = unit->state.formation.center;
    auto frontLeft = BattleModel::GetFrontLeft(unit->formation, center);
    auto offsetRight = unit->formation.towardRight * static_cast<float>(file);
    auto offsetBack = unit->formation.towardBack * static_cast<float>(rank);
    return frontLeft + offsetRight + offsetBack;
  } else {
    auto result = element.state.body.position + element.state.body.velocity * timeStep_;
    auto adjust = glm::vec2{};
    int count = 0;

    const float elementDistance = 0.9f;

    for (auto i = model_->fighterQuadTree.find(result.x, result.y, elementDistance); *i; ++i) {
      auto& obstacle = ***i;
      if (obstacle.get() != &element) {
        auto position = obstacle->state.body.position;
        auto diff = position - result;
        float distance2 = glm::dot(diff, diff);
        if (0.01f < distance2 && distance2 < elementDistance * elementDistance) {
          adjust -= glm::normalize(diff) * elementDistance;
          ++count;
        }
      }
    }

    const float weaponDistance = 0.75f;

    for (auto i = model_->weaponQuadTree.find(result.x, result.y, weaponDistance); *i; ++i) {
      auto& obstacle = ***i;
      if (obstacle->unit->allianceId != unit->allianceId) {
        auto r = obstacle->unit->stats.subunits.front().weapon.melee.weaponReach * vector2_from_angle(obstacle->state.body.bearing);
        auto position = obstacle->state.body.position + r;
        auto diff = position - result;
        if (glm::dot(diff, diff) < weaponDistance * weaponDistance) {
          diff = obstacle->state.body.position - result;
          adjust -= glm::normalize(diff) * weaponDistance;
          ++count;
        }
      }
    }

    if (count != 0) {
      result += adjust / (float)count;
    }

    return result;
  }
}


glm::vec2 BattleSimulator::NextElementVelocity(Element& element) const {
  auto unit = element.unit.get();
  float speed = BattleModel::GetCurrentSpeed(*unit);
  auto destination = element.state.body.destination;

  switch (element.state.melee.readyState) {
    case ReadyState::Striking:
      speed = unit->stats.subunits.front().stats.movement.walkingSpeed / 4.0f;
      break;

    case ReadyState::Stunned:
      speed = unit->stats.subunits.front().stats.movement.walkingSpeed / 4.0f;
      break;

    default:
      break;
  }

  element.terrain.tolerance -= 0.15f;

  if (terrainMap_ && glm::length(element.state.body.position - element.terrain.position) > element.terrain.tolerance) {
    element.terrain.forest = terrainMap_->isForest(element.state.body.position);
    bool impassable = terrainMap_->isImpassable(element.state.body.position);
    if (impassable) {
      float dx = static_cast<float>(rand() & 3) - 1.5f;
      float dy = static_cast<float>(rand() & 3) - 1.5f;
      auto p2 = element.state.body.position + 4.0f * glm::normalize(element.state.body.position - element.terrain.position) + glm::vec2{dx, dy};
      impassable = terrainMap_->isImpassable(p2);
    }
    element.terrain.impassable = impassable;
    element.terrain.tolerance = 4.0f;
    if (impassable) {
      float dx = static_cast<float>(rand() & 3) - 1.5f;
      float dy = static_cast<float>(rand() & 3) - 1.5f;
      element.terrain.position = element.terrain.position + 0.4f * glm::vec2{dx, dy};
    } else {
      element.terrain.position = element.state.body.position;
    }
  }

  if (element.terrain.forest) {
    if (unit->stats.subunits.front().stats.movement.propulsion == PropulsionMode::Quadruped)
      speed *= 0.5f;
    else
      speed *= 0.9f;
  }

  if (element.terrain.impassable) {
    destination = element.terrain.position;
  }

  auto diff = destination - element.state.body.position;
  float diff_len = glm::dot(diff, diff);
  if (diff_len < 0.3f)
    return diff;

  auto delta = glm::normalize(diff) * speed;
  float delta_len = glm::dot(delta, delta);

  return delta_len < diff_len ? delta : diff;
}


WeakPtr<Element> BattleSimulator::FindStrikingTarget(Element& element) const {
  auto unit = element.unit.get();

  auto position = element.state.body.position + unit->stats.subunits.front().weapon.melee.weaponReach * vector2_from_angle(element.state.body.bearing);
  float radius = 1.1f;

  for (auto i = model_->fighterQuadTree.find(position.x, position.y, radius); *i; ++i) {
    auto& target = ***i;
    if (target.get() != &element && target->unit->allianceId != unit->allianceId) {
      return target;
    }
  }

  return nullptr;
}


glm::vec2 BattleSimulator::MovementRules_NextDestination(Element& element, int index) const {
  auto unit = element.unit.get();

  auto& unitState = unit->state;
  auto& fighterState = element.state;

  if (unitState.emotion.IsRouting()) {
    if (GetAlliancePosition(unit->allianceId) == 1)
      return glm::vec2{fighterState.body.position.x * 3, 2000};
    else
      return glm::vec2{fighterState.body.position.x * 3, -2000};
  }

  if (fighterState.melee.opponent) {
    return fighterState.melee.opponent->state.body.position
        - unit->stats.subunits.front().weapon.melee.weaponReach * vector2_from_angle(fighterState.body.bearing);
  }

  switch (fighterState.melee.readyState) {
    case ReadyState::Striking:
    case ReadyState::Stunned:
      return fighterState.body.position;
    default:
      break;
  }

  int rank = index % unit->formation.numberOfRanks;
  int file = index / unit->formation.numberOfRanks;
  auto destination = glm::vec2{};
  if (rank == 0) {
    if (unitState.formation.unitMode == UnitMode::Moving) {
      destination = fighterState.body.position;
      int n = 1;
      for (int i = 1; i <= 5; ++i) {
        auto other = BattleModel::GetElement(*unit, rank, file - i);
        if (!other)
          break;
        destination += other->state.body.position + (float)i * unit->formation.towardRight;
        ++n;
      }
      for (int i = 1; i <= 5; ++i) {
        auto other = BattleModel::GetElement(*unit, rank, file + i);
        if (other == nullptr)
          break;
        destination += other->state.body.position - (float)i * unit->formation.towardRight;
        ++n;
      }
      destination /= n;
      destination -= glm::normalize(unit->formation.towardBack) * BattleModel::GetCurrentSpeed(*unit);
    } else if (unitState.formation.unitMode == UnitMode::Turning) {
      auto frontLeft = BattleModel::GetFrontLeft(unit->formation, unitState.formation.center);
      destination = frontLeft + unit->formation.towardRight * (float)file;
    } else {
      auto frontLeft = BattleModel::GetFrontLeft(unit->formation, unitState.formation.waypoint);
      destination = frontLeft + unit->formation.towardRight * (float)file;
    }
  } else {
    auto elementLeft = BattleModel::GetElement(*unit, rank - 1, file - 1);
    auto elementMiddle = BattleModel::GetElement(*unit, rank - 1, file);
    auto elementRight = BattleModel::GetElement(*unit, rank - 1, file + 1);

    if (elementLeft == nullptr || elementRight == nullptr) {
      destination = elementMiddle->state.body.destination;
    } else {
      destination = (elementLeft->state.body.destination + elementRight->state.body.destination) / 2.0f;
    }
    destination += unit->formation.towardBack;
  }

  return destination;
}


void BattleSimulator::UpdateUnitRange(Unit& unit) {
  auto& unitRange = unit.missileRange;

  unitRange.angleLength = (float)M_PI_2;
  unitRange.angleStart = unit.state.formation.bearing - 0.5f * unitRange.angleLength;

  float minimumRange = 0.0f;
  float maximumRange = 0.0f;
  bool flagTrajectory = false;
  for (const auto& subunit : unit.stats.subunits) {
    if (subunit.weapon.missile.maximumRange != 0.0f) {
      minimumRange = subunit.weapon.missile.minimumRange;
      maximumRange = subunit.weapon.missile.maximumRange;
      flagTrajectory = subunit.weapon.missile.flatTrajectory;
    }
  }


  unitRange.minimumRange = minimumRange;
  unitRange.maximumRange = maximumRange;

  if (unitRange.minimumRange > 0 && unitRange.maximumRange > 0) {
    auto heightMap = terrainMap_ ? &terrainMap_->getHeightMap() : nullptr;
    float centerHeight = (heightMap ? heightMap->interpolateHeight(unit.state.formation.center) : 0.0f) + 1.9f;

    int n = static_cast<int>(unitRange.actualRanges.size()) - 1;
    for (int i = 0; i <= n; ++i) {
      float angle = unitRange.angleStart + i * unitRange.angleLength / n;
      auto direction = vector2_from_angle(angle);
      float delta = (unitRange.maximumRange - unitRange.minimumRange) / 16;
      float maxRange = 0;
      float maxAngle = -100;
      float range = unitRange.minimumRange + delta;
      while (range <= unitRange.maximumRange) {
        float height = (heightMap ? heightMap->interpolateHeight(unit.state.formation.center + range * direction) : 0.0f) + 0.5f;
        float verticalAngle = glm::atan(height - centerHeight, range);
        float tolerance = flagTrajectory ? 0.01f : 0.06f;
        if (verticalAngle > maxAngle - tolerance) {
          maxAngle = verticalAngle;
          maxRange = range;
        }
        range += delta;
      }
      unitRange.actualRanges[i] = glm::max(maxRange, unitRange.minimumRange);
    }
  }
}


void BattleSimulator::TriggerShooting(Unit& unit) {
  if (!unit.unbuffered.deployed)
    return;
  if (unit.state.emotion.IsRouting())
    return;
  if (!unit.missileTarget)
    return;

  for (const auto& subunit : unit.stats.subunits) {
    if (subunit.weapon.missile.maximumRange != 0.0f) {
      ControlAddShooting shooting{};
      shooting.unitId = unit.unitId;
      shooting.missileType = subunit.weapon.missile.id;
      shooting.hitRadius = subunit.weapon.missile.hitRadius;

      auto target = unit.missileTarget->state.formation.center;

      float totalDistance = 0;
      int missileCount = 0;

      for (auto& element : unit.elements) {
        if (element->state.melee.readyState == ReadyState::Prepared) {
          float dx = 10.0f * (static_cast<float>(rand() & 255) / 128.0f - 1.0f);
          float dy = 10.0f * (static_cast<float>(rand() & 255) / 127.0f - 1.0f);

          Projectile projectile{
              element->state.body.position,
              target + glm::vec2{dx, dy},
              subunit.weapon.missile.missileDelay * (static_cast<float>(rand() & 0x7FFF) / (float) 0x7FFF)};
          shooting.projectiles.push_back(projectile);
          totalDistance += glm::length(projectile.position1 - projectile.position2);
          ++missileCount;
          if (subunit.individuals == 0) {
            break;
          }
        }
      }

      shooting.timeToImpact = totalDistance / subunit.weapon.missile.missileSpeed / missileCount;
      shooting.timer = timerDelay_;

      AddShooting(shooting);
    }
  }
}


void BattleSimulator::AddShooting(const ControlAddShooting& command) {
  if (auto unit = FindUnit(command.unitId)) {
    if (const auto* missileStats = unit->FindMissileStats(command.missileType)) {
      Shooting shooting;
      shooting.unitId = unit->object.getObjectId();
      shooting.missileType = command.missileType;
      shooting.maximumRange = missileStats->maximumRange;
      shooting.hitRadius = command.hitRadius;
      shooting.timeToImpact = command.timeToImpact;
      shooting.original = true;
      shooting.projectiles = command.projectiles;

      shootings_.emplace_back(command.timer, shooting);
    }
  }
}


bool BattleSimulator::IsDeploymentZone(ObjectId allianceId, glm::vec2 position) const {
  for (const auto& deploymentZone : battleFederate_->getObjectClass("DeploymentZone")) {
    if (allianceId == deploymentZone["alliance"_ObjectId]) {
      auto p = deploymentZone["position"_vec2];
      float r = deploymentZone["radius"_float];
      if (glm::distance(position, p) < r)
        return true;
    }
  }
  return false;
}


void BattleSimulator::UpdateUnitObjectsFromEntities() {
  for (const auto& unit : model_->units) {
    UpdateUnitObjectFromEntity_Local(*unit);

    unit->remoteUpdateCountdown -= timeStep_;
    if (unit->remoteUpdateCountdown <= 0) {
      UpdateUnitObjectFromEntity_Remote(*unit);
      unit->remoteUpdateCountdown = 0.001f * std::uniform_int_distribution<std::mt19937::result_type>{1000, 5000}(rng_);
    }
  }
}


void BattleSimulator::UpdateUnitObjectFromEntity_Local(Unit& unit) {
  unit.object["_position"] = unit.state.formation.center;
  unit.object["_destination"] = unit.command.path.empty() ? unit.state.formation.center : unit.command.path.back();
  unit.object["_standing"] = unit.state.formation.unitMode == UnitMode::Standing;
  unit.object["_moving"] = unit.state.formation.unitMode == UnitMode::Moving;
  unit.object["_formation"] = FormationToBson(unit.formation);
  unit.object["_path"] = unit.command.path;

  unit.object["_angleStart"] = unit.missileRange.angleStart;
  unit.object["_angleLength"] = unit.missileRange.angleLength;
  unit.object["_rangeValues"] = unit.missileRange.actualRanges;

  auto loadingProgress = unit.state.missile.loadingDuration != 0
      ? std::make_pair(true, unit.state.missile.loadingTimer / unit.state.missile.loadingDuration)
      : std::make_pair(false, 0.0f);
  unit.object["_loading"] = loadingProgress.first;
  unit.object["_loadingProgress"] = loadingProgress.second;

  unit.object["_effectiveMorale"] = unit.state.emotion.GetEffectiveMorale();
  unit.object["_routing"] = unit.state.emotion.IsRouting();


  unit.object["_fighterCount"] = static_cast<int>(unit.elements.size());

  std::vector<glm::vec3> elements{};
  for (auto& element : unit.elements) {
    elements.emplace_back(element->state.body.position, element->state.body.bearing);
  }
  unit.object["_fighters"] = Struct{} << "..." << Binary{elements.data(), elements.size() * sizeof(glm::vec3)} << ValueEnd{};
}


void BattleSimulator::UpdateUnitObjectFromEntity_Remote(Unit& unit) {
  auto commander = battleFederate_->getObject(unit.object["commander"_ObjectId]);
  const char* playerId = commander ? commander["playerId"_c_str] : nullptr;
  bool shouldHaveOwnership = playerId != nullptr && commanderPlayerId_ == playerId;

  if (unit.object["center"].canSetValue() && !unit.object["center"].hasDelayedChange()) {
    unit.object["center"] = unit.state.formation.center;
    //unit.command.centerVersion = unit.object["center"].GetVersion();
  } else if (shouldHaveOwnership && unit.object["center"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["center"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["path"].canSetValue() && !unit.object["path"].hasDelayedChange()) {
    unit.object["path"] = unit.command.path;
    unit.command.pathVersion = unit.object["path"].getVersion();
  } else if (shouldHaveOwnership && unit.object["path"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["path"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["running"].canSetValue() && !unit.object["running"].hasDelayedChange()) {
    unit.object["running"] = unit.command.running;
    unit.command.runningVersion = unit.object["running"].getVersion();
  } else if (shouldHaveOwnership && unit.object["running"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["running"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["facing"].canSetValue() && !unit.object["facing"].hasDelayedChange()) {
    unit.object["facing"] = unit.command.facing;
    unit.command.facingVersion = unit.object["facing"].getVersion();
  } else if (shouldHaveOwnership && unit.object["facing"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["facing"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["meleeTarget"].canSetValue() && !unit.object["meleeTarget"].hasDelayedChange()) {
    unit.object["meleeTarget"] = unit.command.meleeTarget ? unit.command.meleeTarget->unitId : ObjectId{};
    unit.command.meleeTargetVersion = unit.object["meleeTarget"].getVersion();
  } else if (shouldHaveOwnership && unit.object["meleeTarget"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["meleeTarget"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["missileTarget"].canSetValue() && !unit.object["missileTarget"].hasDelayedChange()) {
    unit.object["missileTarget"] = unit.command.missileTarget ? unit.command.missileTarget->unitId : ObjectId{};
    unit.command.missileTargetVersion = unit.object["missileTarget"].getVersion();
  } else if (shouldHaveOwnership && unit.object["missileTarget"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["missileTarget"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["intrinsicMorale"].canSetValue() && !unit.object["intrinsicMorale"].hasDelayedChange()) {
    unit.object["intrinsicMorale"] = unit.state.emotion.intrinsicMorale;
    unit.intrinsicMoraleVersion = unit.object["intrinsicMorale"].getVersion();
  } else if (shouldHaveOwnership && unit.object["intrinsicMorale"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["intrinsicMorale"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["routed"].canSetValue() && !unit.object["routed"].hasDelayedChange()) {
    unit.object["routed"] = unit.state.emotion.IsRouting();
  } else if (shouldHaveOwnership && unit.object["routed"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["routed"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  if (unit.object["fighters"].canSetValue() && !unit.object["fighters"].hasDelayedChange()) {
    if (!unit.elements.empty()) {
      auto remoteElementsArr = Struct{} << "_" << Array{};
      for (auto& element : unit.elements) {
        remoteElementsArr = std::move(remoteElementsArr) << Struct{}
            << "x" << element->state.body.position.x
            << "y" << element->state.body.position.y
            << ValueEnd{};
      }
      auto remoteElementsDoc = std::move(remoteElementsArr) << ValueEnd{} << ValueEnd{};
      unit.object["fighters"] = *remoteElementsDoc.begin();
      unit.fightersVersion = unit.object["fighters"].getVersion();
    } else {
      unit.object["fighters"] = nullptr;
    }
  } else if (shouldHaveOwnership && unit.object["fighters"].getOwnershipState() & OwnershipStateFlag::NotAcquiring) {
    unit.object["fighters"].modifyOwnershipState(OwnershipOperation::OwnershipAcquisition);
  }

  // _commanderPlayerId.clear(); // TODO: make sure to handle multiple session with same player
}


int BattleModel::CountCavalryInMelee() const {
  int result = 0;

  for (const auto& unit : units) {
    if (unit->stats.subunits.front().stats.movement.propulsion == PropulsionMode::Quadruped && IsInMelee(*unit))
      ++result;
  }

  return result;
}


int BattleModel::CountInfantryInMelee() const {
  int result = 0;

  for (const auto& unit : units) {
    if (unit->stats.subunits.front().stats.movement.propulsion == PropulsionMode::Biped && IsInMelee(*unit))
      ++result;
  }

  return result;
}


/***/


WeakPtr<Unit> BattleModel::ClosestEnemyWithinLineOfFire(Unit& unit) const {
  const RootPtr<Unit>* closestEnemy = nullptr;
  float closestDistance = 10000;
  for (const auto& target : units) {
    if (target && target->allianceId != unit.allianceId && BattleModel::IsWithinLineOfFire(unit, target->state.formation.center)) {
      float distance = glm::length(target->state.formation.center - unit.state.formation.center);
      if (distance < closestDistance) {
        closestEnemy = &target;
        closestDistance = distance;
      }
    }
  }
  return closestEnemy ? *closestEnemy : WeakPtr<Unit>{};
}


static float normalize_angle(float a) {
  static float two_pi = 2.0f * (float)M_PI;
  while (a < 0)
    a += two_pi;
  while (a > two_pi)
    a -= two_pi;
  return a;
}


bool BattleModel::IsWithinLineOfFire(Unit& unit, glm::vec2 target) {
  const auto& missileRange = unit.missileRange;
  if (missileRange.minimumRange > 0 && missileRange.maximumRange > 0) {
    glm::vec2 diff = target - unit.state.formation.center;
    float angle = glm::atan(diff.y, diff.x);
    float angleDelta = 0.5f * missileRange.angleLength;

    if (glm::abs(diff_radians(angle, missileRange.angleStart + angleDelta)) > angleDelta)
      return false;

    float distance = glm::length(diff);
    if (distance < missileRange.minimumRange)
      return false;

    if (!missileRange.actualRanges.empty()) {
      float n = missileRange.actualRanges.size() - 1;
      float k = n * normalize_angle(angle - missileRange.angleStart) / missileRange.angleLength;
      float i = glm::floor(k);

      float a0 = missileRange.actualRanges[(int)i];
      float a1 = missileRange.actualRanges[(int)i + 1];
      float actualRange = glm::mix(a0, a1, k - i);

      return distance <= actualRange;
    }
  }

  return false;
}


/***/


ObjectRef BattleSimulator::GetAlliance(ObjectId allianceId) const {
  return battleFederate_->getObject(allianceId);
}


int BattleSimulator::GetAlliancePosition(ObjectId allianceId) const {
  if (auto alliance = GetAlliance(allianceId))
    return alliance["position"_int];
  return 0;
}


ObjectRef BattleSimulator::GetCommanderById(ObjectId commanderId) const {
  return battleFederate_->getObject(commanderId);
}
