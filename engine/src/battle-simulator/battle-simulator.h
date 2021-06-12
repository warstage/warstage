// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_SIMULATOR__BATTLE_SIMULATOR_H
#define WARSTAGE__BATTLE_SIMULATOR__BATTLE_SIMULATOR_H

#include "./battle-objects.h"
#include "battle-model/terrain-map.h"
#include "runtime/runtime.h"
#include <map>
#include <random>
#include <string>

class TerrainMap;


class BattleSimulator :
        public Shutdownable,
        public std::enable_shared_from_this<BattleSimulator>
{
    const float timeStep_{1.0f / 15.0f};
    const double commandDelay_ = 0.25;
    const float timerDelay_ = 0.25;

    std::shared_ptr<Strand> simulatorStrand_;
    std::shared_ptr<Federate> battleFederate_{};
    std::shared_ptr<IntervalObject> interval_{};

    std::vector<std::pair<float, BattleSM::Shooting>> shootings_{};
    std::unordered_map<ObjectId, int> allianceCasualtyCount_{};

    TerrainMap* terrainMap_{};
    std::unordered_map<ObjectId, BackPtr<BattleSM::Unit>> unitLookup_{};

    ObjectRef terrain_{};
    ObjectRef battleStatistics_{};
    
    std::string commanderPlayerId_;

    std::mt19937 rng_{std::random_device{}()};

    std::unique_ptr<BattleSM::BattleModel> model_{};


public:
    explicit BattleSimulator(Runtime& runtime);

    void Startup(ObjectId battleFederationId);

protected: // Shutdownable
    [[nodiscard]] Promise<void> shutdown_() override;

public:
    void Initialize(ObjectId battleFederationId);

private:
    void acquireTerrainMap();
    void releaseTerrainMap();

    WeakPtr<BattleSM::Unit> FindUnit(ObjectId unitId) const;
    void UnitChanged(const ObjectRef& unitObject);
    void DiscoverUnit(const ObjectRef& unitObject);
    RootPtr<BattleSM::Unit> MakeUnit(const ObjectRef& unitObject);
    void DeployUnit(ObjectId unitId, glm::vec2 position, float bearing);
    void RemoveUnit(ObjectId unitId);
    
    void ProcessCommandEvent(const Value& event, float latency);
    void ProcessCommanderEvent(const Value& event);

private:
    void SimulateTimeStep();

    void UpdateUnitEntityFromObject();

    static void MovementRules_AdvanceTime(BattleSM::Unit& unit, float timeStep);
    static void MovementRules_SwapElements(BattleSM::Unit& unit);

    BattleSM::UnitBufferedState NextUnitState(BattleSM::Unit& unit) const;
    float NextUnitDirection(BattleSM::Unit& unit) const;
    BattleSM::UnitMode NextUnitMode(BattleSM::Unit& unit) const;
    bool AllianceHasAbandondedBattle(ObjectId allianceId) const;
    glm::vec2 MovementRules_NextWaypoint(BattleSM::Unit& unit) const;

    BattleSM::ElementState NextElementState(BattleSM::Element& element, int index) const;
    glm::vec2 NextElementPosition(BattleSM::Element& element, int index) const;
    glm::vec2 NextElementVelocity(BattleSM::Element& element) const;
    WeakPtr<BattleSM::Element> FindStrikingTarget(BattleSM::Element& element) const;
    glm::vec2 MovementRules_NextDestination(BattleSM::Element& element, int index) const;

    void UpdateUnitRange(BattleSM::Unit& unit);

    void TriggerShooting(BattleSM::Unit& unit);
    void AddShooting(const BattleSM::ControlAddShooting& command);

    bool IsDeploymentZone(ObjectId allianceId, glm::vec2 position) const;

    void UpdateUnitObjectsFromEntities();
    void UpdateUnitObjectFromEntity_Local(BattleSM::Unit& unit);
    void UpdateUnitObjectFromEntity_Remote(BattleSM::Unit& unit);

private:
    ObjectRef GetAlliance(ObjectId allianceId) const;
    int GetAlliancePosition(ObjectId allianceId) const;
    ObjectRef GetCommanderById(ObjectId commanderId) const;
};


#endif
