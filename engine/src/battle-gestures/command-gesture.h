// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__COMMAND_GESTURE_H
#define WARSTAGE__BATTLE_GESTURES__COMMAND_GESTURE_H

#include "geometry/bounds.h"
#include "battle-simulator/battle-simulator.h"
#include "gesture/gesture.h"
#include "gesture/pointer.h"
#include "battle-audio/sound-director.h"
#include <chrono>

class SoundDirector;
class UnitController;


struct CommandGestureMarker {
    ObjectId unitId{};
    ObjectRef object{};

    bool preliminaryAdded{};
    bool preliminaryRemoved{};
    bool didManeuver{};
    bool running{};
    bool renderSelected{};
    glm::vec2 orientationPoint{};
    bool hasOrientation{};
    bool renderOrientation{};
    ObjectId meleeTargetId{};
    ObjectId missileTargetId{};

    std::vector<glm::vec2> path{};
    std::vector<glm::vec2> adjustedPath{};
    glm::vec2 offset{};

    explicit CommandGestureMarker(ObjectId id) : unitId{id} { }
};


struct UnitGestureGroup {
    ObjectRef object{};

    std::vector<CommandGestureMarker*> unitGestureMarkers{};

    bool selection{};
    bool didManeuver{};

    bool renderSelectionLasso{};
    glm::vec3 selectionAnchor{};
    glm::vec3 selectionPoint{};
};


enum class UnitGestureState {
    None,
    DragDeploy,
    DragSelect,
    PressGround,
    PressCenter,
    PressOrientation,
    DragMovementPath,
    DragMovementLine,
    DragDestination,
    DragOrientation,
};


class CommandGesture : public std::enable_shared_from_this<CommandGesture>, public Gesture {
    UnitController* unitController_{};
    SoundDirector* soundDirector_{};
    int tapCount_{};
    ObjectId lastTappedUnitId_{};
    Pointer* deselectAllTouch_{};
    std::vector<ObjectId> straightMovableUnitIds_{};

    SoundSampleID soundSampleId_{};
    ObjectRef debug2D_{};

public:
    ObjectRef deploymentUnit_{};
    std::unique_ptr<UnitGestureGroup> unitGestureGroup_{};
    CommandGestureMarker* unitGestureMarker_{};
    glm::vec2 offsetOriginal_{};
    glm::vec2 offsetCurrent_{};
    glm::vec2 offsetFactor_{};
    UnitGestureState unitGestureState_{};
    bool allowTargetEnemyUnit_{};
    bool pointerHasMoved_{};
    std::chrono::system_clock::time_point longTapTimer_{};

public:
    CommandGesture(Surface* gestureSurface, UnitController* unitController, SoundDirector* soundDirector);
    void Initialize();

    void ObserveUnit(ObjectRef& unit);

    void Animate() override;

    void PointerWillBegin(Pointer* pointer) override;
    void PointerHasBegan(Pointer* pointer) override;
    void PointerWasMoved(Pointer* pointer) override;
    void PointerWasEnded(Pointer* pointer) override;
    void PointerWasCancelled(Pointer* pointer) override;

private:
    void BeginPointer(Pointer* pointer);
    void BeginUnitGesture(Pointer& pointer, ObjectId unitId, glm::vec3 terrainPosition);
    void BeginDeploymentGestureFromDeploymentUnit(Pointer& pointer, ObjectRef& reinforcementObject);
    void BeginDoubleTap(Pointer& pointer, glm::vec2 terrainPosition);
    
    void UpdateUnitGesture();
    void UpdateUnitMovement(CommandGestureMarker* unitGestureMarker, glm::vec2 fingerPosition, glm::vec2 markerPosition);

    void TouchEndedOrCancelled(Pointer* pointer, bool cancelled);

    bool IssueCommand();

    void RemoveUnitGestureMarker(ObjectId unitId);
    void SetUnitGestureOffsets(glm::vec2 position, bool allowFutureCenter);

    glm::vec2 UpdatePathDuringDeployment(ObjectId allianceId, std::vector<glm::vec2>& path, glm::vec2 markerPosition);
    bool IsDeploymentZone(ObjectId allianceId, glm::vec2 position) const;
    glm::vec2 ConstrainDeploymentZone(ObjectId allianceId, glm::vec2 position, float inset) const;
    void UpdateAdjustedPath(CommandGestureMarker* unitGestureMarker);

    void TryPlaySoundSample();

    void RenderDebug2D();

    void UpdateOffsetToMarker(bool finger);

    [[nodiscard]] bool IsManeuver() const {
        return unitGestureState_ == UnitGestureState::DragMovementPath
            || unitGestureState_ == UnitGestureState::DragMovementLine;
    }
};


#endif
