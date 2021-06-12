// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_MODEL__BATTLE_SM_H
#define WARSTAGE__BATTLE_MODEL__BATTLE_SM_H

#include "geometry/quad-tree.h"
#include "utilities/memory.h"
#include "runtime/runtime.h"
#include <array>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace BattleSM {

  struct Element;
  struct Unit;

  enum class FormationType {
    None,
    Column,
    Line,
    Skirmish,
    Square,
    Wedge
  };

  struct FormationStats {
    FormationType type = {};
    int files = 0;
    int ranks = 0;
    bool testudo = false;
    glm::vec2 spacing = {};  // lateral, frontal
  };

/***/

  struct BodyStats {
    glm::vec2 size = {}; // width, depth
  };

/***/

  enum class PropulsionMode {
    None,
    Biped,
    Quadruped
  };

  struct MovementStats {
    PropulsionMode propulsion = {};
    float walkingSpeed = 0.0f; // meters per second
    float runningSpeed = 0.0f; // meters per second
    float routingSpeed = 0.0f;
  };

  struct ElementStats {
    BodyStats body = {};
    MovementStats movement = {};
  };

/***/

  enum class MissileType {
    None,
    Bow,
    Arq,
    Cannon
  };

  struct MissileStats {
    int id = 0;
    float minimumRange = 0.0f; // minimum fire range in meters
    float maximumRange = 0.0f; // maximum fire range in meters
    bool flatTrajectory = false;
    float missileSpeed = 0.0f;
    float missileDelay = 0.0f;
    float loadingTime = 0.0f;
    float hitRadius = 0.0f;
  };

  struct MeleeStats {
    float weaponReach = 0.0f;
    float strikingDuration = 0.0f;
    float readyingDuration = 0.0f;
  };

  struct WeaponStats {
    MeleeStats melee = {};
    MissileStats missile = {};
  };

/***/

  struct SubunitStats {
    ElementStats stats = {};
    WeaponStats weapon = {};
    int individuals = 0;
  };

  struct UnitStats {
    FormationStats formation = {};
    std::vector<SubunitStats> subunits = {};
    float training = 0.0f;
  };


/***/

  struct Body {
    glm::vec2 position = {};
    glm::vec2 velocity = {};
    glm::vec2 destination = {}; // ???
    float position_z = 0.0f; // ???
    float bearing = 0.0f;
  };


/***/

  enum class ElementType {
    Individual, Vehicle, Weapon
  };

  enum class ReadyState {
    Unready,
    Readying,
    Prepared,
    Striking,
    Stunned
  };

  struct MeleeState {
    ReadyState readyState = {};
    float readyingTimer = 0.0f;
    float strikingTimer = 0.0f;
    float stunnedTimer = 0.0f;
    WeakPtr<Element> opponent = {};
    WeakPtr<Element> target = {};
  };

  struct ElementState {
    Body body = {};
    MeleeState melee = {};
  };

  struct TerrainState {
    glm::vec2 position = {};
    float tolerance = 0.0f;
    bool forest = false;
    bool impassable = false;
  };

  struct Element {
    BackPtr<Unit> unit;
    TerrainState terrain = {};
    ElementState state = {};
    ElementState nextState = {};
    bool casualty{};
  };


  struct Vehicle {
    WeakPtr<Element> vehicleElement;
    WeakPtr<Element> driverElement;
  };

  struct Weapon {
    WeakPtr<Element> weaponElement;
    WeakPtr<Element> wielderElement;
  };

  enum class UnitMode {
    Initializing,
    Standing,
    Moving,
    Turning
  };

  struct MissileState {
    float loadingTimer{};
    float loadingDuration{};
    int shootingCounter{};
  };

  struct EmotionState {
    float intrinsicMorale{1.0f};
    float influence{};

    [[nodiscard]] float GetEffectiveMorale() const { return intrinsicMorale + influence; }

    [[nodiscard]] bool IsRouting() const { return intrinsicMorale + influence <= 0; }
  };

  struct Projectile {
    Projectile(glm::vec2 p1, glm::vec2 p2, float d) : position1{p1}, position2{p2}, delay{d} {}
    glm::vec2 position1{};
    glm::vec2 position2{};
    float delay{};
  };

  struct Shooting {
    ObjectId unitId{};
    int missileType{};
    float maximumRange{};
    float hitRadius{};
    float timeToImpact{};
    bool original{};
    bool released{};
    std::vector<Projectile> projectiles{};
  };

  struct ControlAddShooting {
    ObjectId unitId{};
    int missileType{};
    float hitRadius{};
    float timeToImpact{};
    std::vector<Projectile> projectiles{};
    float timer{};
  };

  struct MissileRange {
    float angleStart{};
    float angleLength{};
    float minimumRange{};
    float maximumRange{};
    std::array<float, 25> actualRanges{};
  };

  struct FormationSlotAssignment {
    int rank;
    int file;
  };

  struct Formation {
    float rankDistance{};
    float fileDistance{};
    int numberOfRanks{}; // updated by UpdateFormation() and AddUnit()
    int numberOfFiles{}; // updated by UpdateFormation() and AddUnit()
    float _direction{};
    glm::vec2 towardRight{};
    glm::vec2 towardBack{};

    void SetDirection(float direction);
  };

  struct FormationState {
    UnitMode unitMode{};
    glm::vec2 center{};
    float bearing{};
    glm::vec2 waypoint{};
  };

  struct UnitBufferedState {
    MissileState missile{};
    EmotionState emotion{};
    FormationState formation{};
    int recentCasualties{};
  };

  struct CommandState {
    std::vector<glm::vec2> path{};
    bool running{};
    float facing{};
    WeakPtr<Unit> meleeTarget{};
    WeakPtr<Unit> missileTarget{}; // set to self to hold fire

    int pathVersion{};
    int runningVersion{};
    int facingVersion{};
    int meleeTargetVersion{};
    int missileTargetVersion{};

    [[nodiscard]] glm::vec2 GetDestination() const {
      return !path.empty() ? path.back() : glm::vec2(512, 512);
    }
  };

  struct UnitUnbufferedState {
    bool deployed{};
    bool canRally{true};
    int shootingCounter{};
    float timeUntilSwapElements{};
  };

  struct Subunit {
  };

  struct Unit {
    ObjectRef object{};
    ObjectId unitId{};
    ObjectId allianceId{};

    UnitStats stats{};

    std::vector<RootPtr<Subunit>> subunits = {};
    std::vector<RootPtr<Vehicle>> vehicles = {};
    std::vector<RootPtr<Weapon>> weapons = {};
    std::vector<RootPtr<Element>> elements = {};

    UnitBufferedState state{};
    UnitBufferedState nextState{};

    UnitUnbufferedState unbuffered{};

    Formation formation{};
    MissileRange missileRange{};
    CommandState command{};
    WeakPtr<Unit> missileTarget{};

    float remoteUpdateCountdown{};
    int intrinsicMoraleVersion{};
    int fightersVersion{};

    const MissileStats* FindMissileStats(int missileType) {
      for (const auto& subunit : stats.subunits) {
        if (subunit.weapon.missile.id == missileType) {
          return &subunit.weapon.missile;
        }
      }
      return nullptr;
    }
  };

/***/

  struct BattleModel {
    std::vector<RootPtr<Unit>> units = {};
    std::vector<RootPtr<Body>> bodies = {};
    QuadTree<RootPtr<Element> *> fighterQuadTree = {0, 0, 1024, 1024};
    QuadTree<RootPtr<Element> *> weaponQuadTree = {0, 0, 1024, 1024};

    [[nodiscard]] static bool IsInMelee(Unit &unit);
    [[nodiscard]] int CountCavalryInMelee() const;
    [[nodiscard]] int CountInfantryInMelee() const;

    [[nodiscard]] static glm::vec2 CalculateUnitCenter(const Unit &unit);
    [[nodiscard]] static float GetCurrentSpeed(const Unit &unit);
    [[nodiscard]] static Element *GetElement(const Unit &unit, int rank, int file);

    [[nodiscard]] static glm::vec2 GetFrontLeft(const Formation &formation, glm::vec2 center);

    WeakPtr<Unit> ClosestEnemyWithinLineOfFire(Unit &unit) const;
    static bool IsWithinLineOfFire(Unit &unit, glm::vec2 target);
  };

} // namespace BattleSM

#endif
