// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./command-gesture.h"
#include "./camera-control.h"
#include "./unit-controller.h"
#include "battle-audio/sound-director.h"
#include "battle-simulator/convert-value.h"
#include "gesture/surface.h"


#define KEEP_ORIENTATION_TRESHHOLD 40 // meters


CommandGesture::CommandGesture(Surface* gestureSurface, UnitController* unitController, SoundDirector* soundDirector)
    : Gesture{gestureSurface},
    unitController_{unitController},
    soundDirector_{soundDirector} {

    //_debug2D = unitController->_battle_mainThread->Objects("DebugScreen").Create();
}


void CommandGesture::Initialize() {
    auto weak_ = weak_from_this();
  unitController_->battleFederate_->getObjectClass("Unit").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      this_->ObserveUnit(object);
    }
  });

  unitController_->battleFederate_->getObjectClass("DeploymentUnit").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      if (object.justDestroyed() && object == this_->deploymentUnit_) {
        this_->deploymentUnit_ = {};
      }
    }
  });

    unitGestureGroup_ = unitController_->MakeUnitGestureGroup();
}


void CommandGesture::ObserveUnit(ObjectRef& unit) {
    if (unit.justDestroyed()) {
        RemoveUnitGestureMarker(unit.getObjectId());
        unitController_->DeleteEmptyGestureGroups();
    }
}


void CommandGesture::Animate() {
    LOG_ASSERT(Strand::getMain()->isCurrent());

    if (unitController_->acquireTerrainMap()) {
        UpdateUnitGesture();
        unitController_->UpdateRuntimeObjects();
        if (debug2D_)
            RenderDebug2D();
    }
    unitController_->releaseTerrainMap();
    TryPlaySoundSample();
}


void CommandGesture::PointerWillBegin(Pointer* pointer) {
    SubscribePointer(pointer);

    if (!HasCapturedPointers()) {
        deploymentUnit_ = ObjectRef{};
        //_unitGestureGroup = nullptr;
        unitGestureMarker_ = nullptr;
        unitGestureState_ = UnitGestureState::None;
        allowTargetEnemyUnit_ = false;
        pointerHasMoved_ = false;
        longTapTimer_ = std::chrono::system_clock::time_point{};

        if (pointer->GetTapCount() > 1)
            ++tapCount_;
        else
            tapCount_ = 1;
    }
}


void CommandGesture::PointerHasBegan(Pointer* pointer) {
    if (HasCapturedPointers() || pointer->IsCaptured()) {
        return;
    }

    if (unitController_->acquireTerrainMap()) {
        BeginPointer(pointer);
    }
    unitController_->releaseTerrainMap();
    TryPlaySoundSample();
}



void CommandGesture::PointerWasMoved(Pointer* pointer) {
    if (unitController_->acquireTerrainMap()) {
        if (!pointerHasMoved_ && pointer->HasMoved()) {
            pointerHasMoved_ = true;
            for (auto unitGestureMarker : unitController_->GetUnitGestureMarkers()) {
                unitGestureMarker->preliminaryRemoved = false;
            }
        }
        if (HasCapturedPointer(pointer)) {
            UpdateOffsetToMarker(pointer->GetPointerType() == PointerType::Touch);

            if (pointer->GetCurrentButtons() != pointer->GetPreviousButtons()) {
                pointer->ResetHasMoved();
            }

            bool preventLongTap = pointer->HasMoved();

            if (unitGestureState_ == UnitGestureState::PressCenter) {
                float treshhold = 32.0f * GetGestureSurface().GetViewport().getScaling();
                if (glm::distance(pointer->GetOriginalPosition(), pointer->GetCurrentPosition()) > treshhold) {
                    unitGestureState_ = UnitGestureState::DragMovementPath;
                    preventLongTap = true;
                }
            }

            if (preventLongTap) {
                longTapTimer_ = std::chrono::system_clock::time_point{};
                if (unitGestureGroup_->selection && unitGestureGroup_->unitGestureMarkers.size() == 1) {
                    unitGestureGroup_->selection = false;
                }
            }

            UpdateUnitGesture();
            unitController_->UpdateRuntimeObjects();
        }
    }
    unitController_->releaseTerrainMap();
    TryPlaySoundSample();
}


void CommandGesture::PointerWasEnded(Pointer* pointer) {
    TouchEndedOrCancelled(pointer, false);
}


void CommandGesture::PointerWasCancelled(Pointer* pointer) {
    TouchEndedOrCancelled(pointer, true);
}


void CommandGesture::BeginPointer(Pointer* pointer) {
    auto screenPosition = pointer->GetCurrentPosition();
    auto terrainPosition = unitController_->GetCameraControl()->GetTerrainPosition3(screenPosition);
    
    if (auto unitId = unitController_->FindCommandableUnit(screenPosition, terrainPosition)) {
        BeginUnitGesture(*pointer, unitId, terrainPosition);
    
    } else if (auto deploymentUnit = unitController_->GetNearestDeploymentUnit(screenPosition)) {
        BeginDeploymentGestureFromDeploymentUnit(*pointer, deploymentUnit);
    
    } else if (tapCount_ > 1 && !straightMovableUnitIds_.empty()) {
        BeginDoubleTap(*pointer, terrainPosition);
        
    } else {
        deselectAllTouch_ = pointer;
        for (auto unitGestureMarker : unitController_->GetUnitGestureMarkers()) {
            if (!HasCapturedPointers())
                unitGestureMarker->preliminaryRemoved = true;
        }
        if (tapCount_ > 1) {
            unitGestureState_ = UnitGestureState::PressGround;
            longTapTimer_ = std::chrono::system_clock::now() + std::chrono::milliseconds{420};
        }
    }
    
    unitController_->UpdateRuntimeObjects();
    unitController_->UpdateCommandButtons();
}


void CommandGesture::BeginUnitGesture(Pointer& pointer, ObjectId unitId, glm::vec3 terrainPosition) {
    if (auto unitObject = unitController_->GetUnitObject(unitId)) {
        if (!HasCapturedPointer() && TryCapturePointer(&pointer)) {
            auto screenPosition = pointer.GetCurrentPosition();
            auto unitCenterPos = unitController_->GetUnitBounds(unitObject["_position"_vec2]).mid();
            auto destinationPos = unitController_->GetUnitBounds(unitObject["_destination"_vec2]).mid();
            auto orientationPos = unitController_->GetUnitModifierBounds(unitId).mid();
            bool hasDestinationPos = glm::distance(unitCenterPos, destinationPos) > 12.0;
            bool hasOrientationPos = glm::distance(orientationPos, destinationPos) > 12.0;
            float distanceToUnitCenter = glm::distance(unitCenterPos, screenPosition);
            float distanceToDestination = hasDestinationPos ? glm::distance(destinationPos, screenPosition) : 99999;
            float distanceToOrientation = hasOrientationPos ? glm::distance(orientationPos, screenPosition) : 99999;
            float distanceMinimum = glm::min(distanceToUnitCenter, glm::min(distanceToDestination, distanceToOrientation));

            auto unitGestureState
                = distanceToUnitCenter == distanceMinimum ? UnitGestureState::PressCenter
                    : distanceToDestination == distanceMinimum ? UnitGestureState::DragDestination
                        : distanceToOrientation == distanceMinimum ? UnitGestureState::PressOrientation
                            : UnitGestureState::None;

            auto unitPos2 = unitGestureState != UnitGestureState::PressCenter ? unitObject["_destination"_vec2] : unitObject["_position"_vec2];
            auto unitPos3 = unitController_->getHeightMap().getPosition(unitPos2, 0.0f);
            auto screenOffset = unitController_->GetCameraControl()->ContentToWindow(unitPos3) - screenPosition;

            if (unitId != lastTappedUnitId_)
                tapCount_ = 1;

            auto tappedGestureMarker = unitController_->FindUnitGestureMarker(unitId);
            if (tappedGestureMarker) {
                if (HasCapturedPointer() && GetCapturedPointer() != &pointer) {
                    // unit is already part of a battle gesture
                    ReleasePointer(&pointer);
                    unitController_->releaseTerrainMap();
                    TryPlaySoundSample();
                    return;
                }
                if (unitGestureState == UnitGestureState::PressCenter && unitGestureGroup_->selection && tapCount_ == 1) {
                    tappedGestureMarker->preliminaryRemoved = true;
                }
            } else {
                unitGestureGroup_->selectionAnchor = terrainPosition;
                tappedGestureMarker = unitController_->AddUnitGestureMarker(unitId);
                tappedGestureMarker->preliminaryAdded = true;
                if (unitGestureGroup_->selection)
                    soundSampleId_ = SoundSampleID::TapSelect;
            }

            lastTappedUnitId_ = tappedGestureMarker->unitId;
            unitGestureMarker_ = tappedGestureMarker;
            allowTargetEnemyUnit_ = unitObject["stats.isMissile"_bool];
            unitGestureState_ = unitGestureState;
            offsetFactor_ = glm::vec2{};
            offsetOriginal_ = screenOffset;
            offsetCurrent_ = screenOffset;

            SetUnitGestureOffsets(unitPos2, true);

            bool shouldDeselect = !unitGestureGroup_->selection
                || (unitGestureState == UnitGestureState::PressCenter && unitGestureGroup_->selection && tapCount_ > 1)
                || unitGestureState == UnitGestureState::DragDestination
                || unitGestureState == UnitGestureState::PressOrientation
                || (unitGestureGroup_->didManeuver && !tappedGestureMarker->didManeuver);

            if (shouldDeselect) {
                auto unitGestureMarkers = unitController_->GetUnitGestureMarkers();
                for (auto unitGestureMarker : unitGestureMarkers) {
                    if (unitGestureMarker != tappedGestureMarker) {
                        unitController_->DeleteUnitGestureMarker(unitGestureMarker);
                    }
                }
                unitGestureGroup_->selection = false;
                unitGestureGroup_->didManeuver = false;
            }

            if (unitGestureState == UnitGestureState::PressCenter && (tapCount_ > 1 || !unitGestureGroup_->selection)) {
                longTapTimer_ = std::chrono::system_clock::now() + std::chrono::milliseconds{420};
            }

            tappedGestureMarker->renderSelected = true; //unitGestureState != UnitGestureState::PressOrientation;

            tappedGestureMarker->path.clear();
            if (unitGestureState != UnitGestureState::PressCenter || pointer.GetCurrentButtons().right) {
                for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                    if (auto unitObject1 = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
                        unitGestureMarker->path = DecodeArrayVec2(unitObject1["_path"_value]);
                    }
                }
            }

            for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                if (auto unitObject1 = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
                    bool isRunning = unitObject1["running"_bool];
                    switch (unitGestureState_) {
                        case UnitGestureState::PressOrientation:
                        case UnitGestureState::DragDestination:
                            unitGestureMarker->running = isRunning;
                            break;
                        default:
                            unitGestureMarker->running = tapCount_ > 1;
                            break;
                    }
                }
            }
            UpdateUnitGesture();
        }
    }
}


void CommandGesture::BeginDeploymentGestureFromDeploymentUnit(Pointer& pointer, ObjectRef& deploymentUnit) {
    if (!HasCapturedPointer() && TryCapturePointer(&pointer)) {
        unitGestureState_ = UnitGestureState::DragDeploy;
        deploymentUnit_ = deploymentUnit;
        deploymentUnit_["dragging"] = true;
    }
}


void CommandGesture::BeginDoubleTap(Pointer& pointer, glm::vec2 terrainPosition) {
        if (!HasCapturedPointer() && TryCapturePointer(&pointer)) {
            CommandGestureMarker* tappedGestureMarker = nullptr;
            float distance = 0;
            for (ObjectId unitId : straightMovableUnitIds_) {
                auto unitGestureMarker = unitController_->FindUnitGestureMarker(unitId);
                if (!unitGestureMarker) {
                    unitGestureMarker = unitController_->AddUnitGestureMarker(unitId);
                    unitGestureMarker->renderSelected = true;
                    unitGestureMarker->running = tapCount_ > 2;
                    auto unitObject = unitController_->GetUnitObject(unitId);
                    auto unitPos = unitObject["_position"_vec2];
                    float d = glm::distance(unitPos, terrainPosition);
                    if (!tappedGestureMarker || d < distance) {
                        distance = d;
                        tappedGestureMarker = unitGestureMarker;
                    }
                }
            }
            
            if (tappedGestureMarker) {
                unitGestureGroup_->selection = unitGestureGroup_->unitGestureMarkers.size() > 1;
                
                tapCount_ = 1;
                unitGestureMarker_ = tappedGestureMarker;
                allowTargetEnemyUnit_ = false;
                unitGestureState_ = UnitGestureState::DragMovementLine;
                offsetFactor_ = glm::vec2{};
                offsetOriginal_ = glm::vec2{};
                offsetCurrent_ = glm::vec2{};
                
                auto unitObject = unitController_->GetUnitObject(tappedGestureMarker->unitId);
                auto unitPos = unitObject["_position"_vec2];
                SetUnitGestureOffsets(unitPos, false);
                UpdateUnitGesture();
            }
    }
}


void CommandGesture::UpdateUnitGesture() {
    if (unitGestureState_ == UnitGestureState::PressGround) {
        for (auto pointer : GetSubscribedPointers()) {
            if (pointer->HasMoved()) {
                unitGestureState_ = UnitGestureState::None;
            } else if (std::chrono::system_clock::now() > longTapTimer_) {
                if (!HasCapturedPointer() && TryCapturePointer(pointer)) {
                    auto screenPosition = pointer->GetCurrentPosition();
                    auto terrainPosition = unitController_->GetCameraControl()->GetTerrainPosition3(screenPosition);
                    longTapTimer_ = std::chrono::system_clock::time_point{};
                    unitGestureState_ = UnitGestureState::DragSelect;
                    unitGestureGroup_->selectionAnchor = terrainPosition;
                    soundSampleId_ = SoundSampleID::TapSelectMarker;
                } else {
                    unitGestureState_ = UnitGestureState::None;
                }
            }
        }
    }

    if (auto pointer = GetCapturedPointer()) {
        auto screenFingerPosition = pointer->GetCurrentPosition();
        auto screenMarkerPosition = screenFingerPosition + offsetCurrent_;
        auto fingerPosition = unitController_->GetCameraControl()->GetTerrainPosition3(screenFingerPosition).xy();
        auto markerPosition = unitController_->GetCameraControl()->GetTerrainPosition3(screenMarkerPosition);

        if (unitGestureState_ == UnitGestureState::PressCenter) {
            if (longTapTimer_ != std::chrono::system_clock::time_point{} && std::chrono::system_clock::now() > longTapTimer_) {
                longTapTimer_ = std::chrono::system_clock::time_point{};
                if (tapCount_ > 1) {
                    unitGestureState_ = UnitGestureState::DragSelect;
                    unitGestureGroup_->selectionAnchor = markerPosition;
                    soundSampleId_ = SoundSampleID::TapSelectMarker;
                } else {
                    unitGestureGroup_->selection = true;
                    soundSampleId_ = SoundSampleID::TapSelect;
                }
            }

            if (auto unitObject = unitController_->GetUnitObject(unitGestureMarker_->unitId)) {
                auto path = DecodeArrayVec2(unitObject["_path"_value]);
                if (path.size() <= 2)
                    UpdateUnitMovement(unitGestureMarker_, fingerPosition, markerPosition.xy());
            }
        } else if (unitGestureState_ == UnitGestureState::PressOrientation) {
            if (pointer->HasMoved())
                unitGestureState_ = UnitGestureState::DragOrientation;
        }

        if (unitGestureState_ == UnitGestureState::DragDeploy) {
            if (deploymentUnit_) {
                bool outsideMap = glm::distance(fingerPosition, {512.0f, 512.0f}) > 512.0f;
                if (outsideMap && deploymentUnit_["deletable"_bool]) {
                    deploymentUnit_["_deleting"] = true;
                    deploymentUnit_["_position"] = fingerPosition;
                    deploymentUnit_["_path"] = nullptr;
                } else {
                    auto destination = deploymentUnit_["_position"_value].has_value()
                        ? deploymentUnit_["_position"_vec2]
                        : deploymentUnit_["position"_vec2];
                    destination = unitController_->ConstrainImpassable(destination, fingerPosition);
                    destination = unitController_->ConstrainToContent(destination);

                    auto deploymentZoneId = deploymentUnit_["deploymentZone"_ObjectId];
                    auto position = deploymentZoneId
                        ? unitController_->ConstrainToDeploymentZone(destination, deploymentZoneId)
                        :  destination;

                    if (position != destination) {
                        auto path = DecodeArrayVec2(deploymentUnit_["_path"_value]);
                        UpdateMovementPath(path, path.empty() ? position : path[0], destination, 10.0f);
                        deploymentUnit_["_deleting"] = false;
                        deploymentUnit_["_position"] = path.empty() ? position : path[0];
                        deploymentUnit_["_path"] = path;
                    } else {
                        deploymentUnit_["_deleting"] = false;
                        deploymentUnit_["_position"] = position;
                        deploymentUnit_["_path"] = nullptr;
                    }
                }
            }

        } else if (unitGestureState_ == UnitGestureState::DragOrientation || pointer->GetCurrentButtons().right) {
            for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                if (auto unitObject = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
                    UpdateMovementPathStart(unitGestureMarker->path, unitObject["_position"_vec2], 20);
                    UpdateAdjustedPath(unitGestureMarker);
                }
            }

            if (auto unitGestureMarker = unitGestureMarker_) {
                if (auto unitObject1 = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
        
                    unitGestureMarker->renderOrientation = true;
        
                    auto enemyUnit = unitController_->FindEnemyUnit(fingerPosition, markerPosition.xy());
        
                    bool holdFire = false;
                    if (unitObject1["_standing"_bool] && unitObject1["stats.isMissile"_bool]) {
                        auto unitCurrentBounds = unitController_->GetUnitBounds(unitObject1["_position"_vec2]);
                        holdFire = glm::distance(screenMarkerPosition, unitCurrentBounds.mid()) <= unitCurrentBounds.x().radius();
                    }
        
                    if (holdFire) {
                        unitGestureMarker->missileTargetId = unitGestureMarker->unitId;
                        unitGestureMarker->hasOrientation = false;
                    } else {
                        //if (!_tappedUnitCenter)
                        //	enemyUnit = nullptr;
                        if (!allowTargetEnemyUnit_)
                            enemyUnit = ObjectId{};
            
                        if (enemyUnit && !unitGestureMarker->missileTargetId)
                            soundSampleId_ = SoundSampleID::TapTarget;
            
                        unitGestureMarker->missileTargetId = enemyUnit;
                        unitGestureMarker->hasOrientation = true;
                        unitGestureMarker->orientationPoint = markerPosition.xy();
                    }
                }
            }

        } else if (unitGestureState_ == UnitGestureState::DragSelect) {
            unitGestureGroup_->selection = true;
            unitGestureGroup_->renderSelectionLasso = true;
            unitGestureGroup_->selectionPoint = markerPosition;

            auto p1 = screenMarkerPosition;
            auto p2 = unitController_->GetCameraControl()->GetCameraState().ContentToWindow(unitGestureGroup_->selectionAnchor);
            bounds2f lassoBounds{std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::max(p1.x, p2.x), std::max(p1.y, p2.y)};
            
            std::vector<CommandGestureMarker*> deselect{};
            for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                auto unitId = unitGestureMarker->object["unit"_ObjectId];
                auto unitObject = unitController_->GetUnitObject(unitId);
                auto unitBounds = unitController_->GetUnitIconViewportBounds(unitObject["_position"_vec2]);
                if (!lassoBounds.intersects(unitBounds)) {
                    deselect.push_back(unitGestureMarker);
                }
            }
            
            for (auto unitGestureMarker : deselect) {
                unitController_->DeleteUnitGestureMarker(unitGestureMarker);
            }
            
            for (auto unitId : unitController_->GetUnitIds()) {
                auto unitObject = unitController_->GetUnitObject(unitId);
                if (unitController_->IsCommandableUnit(unitObject)) {
                    auto unitBounds = unitController_->GetUnitIconViewportBounds(unitObject["_position"_vec2]);
                    if (lassoBounds.intersects(unitBounds)) {
                        if (!unitController_->FindUnitGestureMarker(unitId)) {
                            auto unitGestureMarker = unitController_->AddUnitGestureMarker(unitId);
                            unitGestureMarker->preliminaryAdded = true;
                            unitGestureMarker->renderSelected = true;
                            soundSampleId_ = SoundSampleID::TapSelect;
                        }
                    }
                }
            }

        } else if (unitGestureState_ == UnitGestureState::DragMovementPath
            || unitGestureState_ == UnitGestureState::DragMovementLine
            || unitGestureState_ == UnitGestureState::DragDestination) {
            for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                UpdateUnitMovement(unitGestureMarker, fingerPosition, markerPosition.xy());
            }
        }
    }
}


void CommandGesture::UpdateUnitMovement(CommandGestureMarker* unitGestureMarker, glm::vec2 fingerPosition, glm::vec2 markerPosition) {
    if (auto unit = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
        glm::vec2 fingerPos = fingerPosition + unitGestureMarker->offset;
        glm::vec2 markerPos = markerPosition + unitGestureMarker->offset;

        unitGestureMarker->renderOrientation = false;

        glm::vec2 currentDestination = !unitGestureMarker->path.empty() ? unitGestureMarker->path.back() : unit["_position"_vec2];

        markerPos = unitController_->ConstrainToContent(markerPos);
        markerPos = unitController_->ConstrainImpassable(currentDestination, markerPos);

        glm::vec2 unitCenter = unit["_position"_vec2];

        auto enemyUnit = unitController_->FindEnemyUnit(fingerPos, markerPos);
        if (enemyUnit && !unitGestureMarker->meleeTargetId)
            soundSampleId_ = SoundSampleID::TapCharge;

        unitGestureMarker->meleeTargetId = enemyUnit;

        if (unitGestureState_ == UnitGestureState::DragMovementLine)
            unitGestureMarker->path.clear();

        UpdateMovementPath(unitGestureMarker->path, unitCenter, markerPos, 20);
        UpdateAdjustedPath(unitGestureMarker);

        auto enemyModelState = unitController_->GetUnitObject(enemyUnit);
        if (enemyModelState) {
            UpdateMovementPath(unitGestureMarker->adjustedPath, unitCenter, enemyModelState["_position"_vec2], 20);
            auto destination = enemyModelState["_position"_vec2];
            auto orientation = destination + glm::normalize(destination - unitCenter) * 18.0f;
            unitGestureMarker->hasOrientation = true;
            unitGestureMarker->orientationPoint = orientation;
        } else if (MovementPathLength(unitGestureMarker->path) > KEEP_ORIENTATION_TRESHHOLD) {
            auto dir = unitGestureMarker->path.size() >= 2
                ? *(unitGestureMarker->path.end() - 1) - *(unitGestureMarker->path.end() - 2)
                : markerPos - unitCenter;
            auto orientation = markerPos + 18.0f * glm::normalize(dir);
            unitGestureMarker->hasOrientation = true;
            unitGestureMarker->orientationPoint = orientation;
        } else {
            auto orientation = markerPos + 18.0f * vector2_from_angle(unit["facing"_float]);
            unitGestureMarker->hasOrientation = true;
            unitGestureMarker->orientationPoint = orientation;
        }
    }
}


void CommandGesture::TouchEndedOrCancelled(Pointer* pointer, bool cancelled) {
    bool needUpdate = false;

    straightMovableUnitIds_.clear();

    auto unitGestureMarkers = unitController_->GetUnitGestureMarkers();
    if (cancelled) {
        for (auto unitGestureMarker : unitGestureMarkers)
            if (unitGestureMarker->preliminaryAdded) {
                unitController_->DeleteUnitGestureMarker(unitGestureMarker);
                needUpdate = true;
            }
    } else {
        for (auto unitGestureMarker : unitGestureMarkers)
            if (unitGestureMarker->preliminaryRemoved) {
                soundSampleId_ = SoundSampleID::TapDeactivate;
                if (pointer == deselectAllTouch_)
                    straightMovableUnitIds_.push_back(unitGestureMarker->unitId);
                unitController_->DeleteUnitGestureMarker(unitGestureMarker);
                needUpdate = true;
            }
    }

    if (pointer == deselectAllTouch_)
        deselectAllTouch_ = nullptr;

    for (auto unitGestureMarker : unitController_->GetUnitGestureMarkers()) {
        unitGestureMarker->preliminaryAdded = false;
        unitGestureMarker->preliminaryRemoved = false;
    }

    if (unitController_->acquireTerrainMap()) {
            if (HasCapturedPointer(pointer)) {
                if (unitGestureState_ == UnitGestureState::DragDeploy) {
                    if (deploymentUnit_) {
                        if (deploymentUnit_["_deleting"_bool]) {
                          unitController_->battleFederate_->getEventClass("DeployUnit").dispatch(Struct{}
                              << "deploymentUnit" << deploymentUnit_.getObjectId()
                              << "deleted" << true
                              << ValueEnd{});
                        } else {
                          unitController_->battleFederate_->getEventClass("DeployUnit").dispatch(Struct{}
                              << "deploymentUnit" << deploymentUnit_.getObjectId()
                              << "position" << deploymentUnit_["_position"_value]
                              << "path" << deploymentUnit_["_path"_value]
                              << ValueEnd{});
                        }
                        deploymentUnit_["dragging"] = false;
                        deploymentUnit_["_deleting"] = false;
                        deploymentUnit_["_position"] = nullptr;
                        deploymentUnit_["_path"] = nullptr;
                        deploymentUnit_ = ObjectRef{};
                    }

                } else /*if (auto unitGestureGroup = _unitGestureGroup)*/ {
                    bool maneuver = false;
                    if (!cancelled) {
                        maneuver = IssueCommand();
                    }

                    for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
                        unitGestureMarker->didManeuver = maneuver;
                        unitGestureMarker->renderOrientation = false;
                        unitGestureMarker->path.clear();
                        unitGestureMarker->adjustedPath.clear();
                    }

                    unitGestureGroup_->didManeuver = maneuver;
                    unitGestureGroup_->renderSelectionLasso = false;

                    needUpdate = true;
                }
            }
    }
    unitController_->releaseTerrainMap();
    TryPlaySoundSample();

    if (unitController_->DeleteEmptyGestureGroups())
        needUpdate = true;

    if (needUpdate)
        unitController_->UpdateRuntimeObjects();

    unitController_->UpdateCommandButtons();
}


bool CommandGesture::IssueCommand() {
    bool maneuver = IsManeuver();
    for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
        if (auto unitObject = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
            auto builder = Struct{}
                << "unit" << unitGestureMarker->unitId;

            if (unitGestureState_ == UnitGestureState::PressCenter) {
                if (tapCount_ > 1) {
                    auto unit = unitController_->battleFederate_->getObject(unitGestureMarker->unitId);
                    auto path = DecodeArrayVec2(unit["_path"].getValue());
                    bool facing = false;
                    if (path.size() >= 2) {
                        auto delta = path[1] - path[0];
                        if (glm::length(delta) >= 1) {
                            facing = true;
                            builder = std::move(builder)
                                << "path" << Array{} << ValueEnd{}
                                << "meleeTarget" << ObjectId{}
                                << "facing" << angle(delta);
                        }
                    }
                    if (!facing) {
                        builder = std::move(builder)
                            << "path" << Array{} << ValueEnd{}
                            << "meleeTarget" << ObjectId{}
                            << "running" << false;
                    }
                    maneuver = true;
                    soundSampleId_ = SoundSampleID::TapMovementDone;
                } else if (!unitGestureMarker->adjustedPath.empty() && unitGestureMarker->adjustedPath.size() <= 2) {
                    builder = std::move(builder)
                        << "path" << unitGestureMarker->adjustedPath
                        << "meleeTarget" << unitGestureMarker->meleeTargetId;
                }
            } else {
                if (unitGestureState_ == UnitGestureState::DragMovementLine
                    || unitGestureState_ == UnitGestureState::DragMovementPath
                    || unitGestureState_ == UnitGestureState::DragDestination) {
                    builder = std::move(builder)
                        << "path" << unitGestureMarker->adjustedPath
                        << "meleeTarget" << unitGestureMarker->meleeTargetId;

                    maneuver = true;
                    soundSampleId_ = SoundSampleID::TapMovementDone;

                    bool isRunning = unitObject["running"_bool];
                    bool shouldRun = isRunning;
                    if (unitGestureState_ == UnitGestureState::DragDestination && !pointerHasMoved_) {
                        shouldRun = tapCount_ > 1 ? true : !isRunning;
                    } else if (maneuver) {
                        shouldRun = unitGestureMarker->running;
                    }
                    if (shouldRun != isRunning)
                        builder = std::move(builder) << "running" << shouldRun;
                }

                if (auto missileTarget = unitController_->GetUnitObject(unitGestureMarker->missileTargetId)) {
                    if (unitGestureMarker->missileTargetId != unitGestureMarker->unitId) {
                        float facing = unitGestureMarker->adjustedPath.empty()
                            ? angle(missileTarget["_position"_vec2] - unitObject["_position"_vec2])
                            : angle(missileTarget["_position"_vec2] - unitGestureMarker->adjustedPath.back());

                        builder = std::move(builder)
                            << "missileTarget" << unitGestureMarker->missileTargetId
                            << "facing" << facing;
                        soundSampleId_ = SoundSampleID::TapTarget;
                    }
                } else if (unitGestureMarker->hasOrientation) {
                    auto p = unitGestureMarker->adjustedPath.empty() ? unitObject["_position"_vec2] : unitGestureMarker->adjustedPath.back();
                    builder = std::move(builder) << "facing" << angle(unitGestureMarker->orientationPoint - p);
                    //_soundSampleId = SoundSampleID::TapMovementDone;
                }
            }

            unitController_->DispatchCommandEvent(std::move(builder) << ValueEnd{});
        }
    }
    return maneuver;
}


/***/


void CommandGesture::RemoveUnitGestureMarker(ObjectId unitId) {
    for (auto unitGestureMarker : unitController_->GetUnitGestureMarkers())
        if (unitGestureMarker->unitId == unitId) {
            unitController_->DeleteUnitGestureMarker(unitGestureMarker);
            break;
        }
}


void CommandGesture::SetUnitGestureOffsets(glm::vec2 position, bool allowFutureCenter) {
    for (auto unitGestureMarker : unitGestureGroup_->unitGestureMarkers) {
        if (auto unitObject = unitController_->GetUnitObject(unitGestureMarker->unitId)) {
            if (unitGestureMarker != unitGestureMarker_) {
                glm::vec2 offset1 = unitObject["_position"_vec2] - position;
                glm::vec2 offset2 = unitObject["_destination"_vec2] - position;
                
                float length1 = offset1.x * offset1.x + offset1.y * offset1.y;
                float length2 = offset2.x * offset2.x + offset2.y * offset2.y;
                bool currentCenter = !allowFutureCenter || length1 < length2;
                unitGestureMarker->offset = currentCenter ? offset1 : offset2;
                if (currentCenter)
                    unitGestureMarker->path.clear();
            } else {
                unitGestureMarker->offset = glm::vec2{};
            }
        }
    }
}


glm::vec2 CommandGesture::UpdatePathDuringDeployment(ObjectId allianceId, std::vector<glm::vec2>& path, glm::vec2 markerPosition) {
    glm::vec2 unitCenter;

    if (IsDeploymentZone(allianceId, markerPosition)) {
        unitCenter = ConstrainDeploymentZone(allianceId, markerPosition, 10);
        path.clear();
    } else {
        while (!path.empty() && IsDeploymentZone(allianceId, path.front()))
            path.erase(path.begin());

        unitCenter = ConstrainDeploymentZone(allianceId, !path.empty() ? path.front() : markerPosition, 10);

        if (!path.empty())
            UpdateMovementPath(path, unitCenter, path.back(), 20);
    }

    return unitCenter;
}


bool CommandGesture::IsDeploymentZone(ObjectId allianceId, glm::vec2 position) const {
    for (const auto& deploymentZone : unitController_->battleFederate_->getObjectClass("DeploymentZone")) {
        if (allianceId == deploymentZone["alliance"_ObjectId]) {
            auto p = deploymentZone["position"_vec2];
            float r = deploymentZone["radius"_float];
            if (glm::distance(position, p) < r)
                return true;
        }
    }
    return false;
}


static glm::vec2 ConstrainToCircle(glm::vec2 position, glm::vec2 center, float radius) {
    auto offset = position - center;
    return glm::length(offset) <= radius ? position : center + radius * glm::normalize(offset);
}


glm::vec2 CommandGesture::ConstrainDeploymentZone(ObjectId allianceId, glm::vec2 position, float inset) const {
    glm::vec2 deploymentZonePosition;
    float deploymentZoneRadius = 0;
    float deploymentZoneDistance = 0;
    for (const auto& deploymentZone : unitController_->battleFederate_->getObjectClass("DeploymentZone")) {
        if (allianceId == deploymentZone["alliance"_ObjectId]) {
            auto p = deploymentZone["position"_vec2];
            float r = deploymentZone["radius"_float];
            float d = glm::distance(position, p);
            if (deploymentZoneDistance == 0 || d < deploymentZoneDistance) {
                deploymentZoneDistance = d;
                deploymentZonePosition = p;
                deploymentZoneRadius = r;
            }
        }
    }
    if (deploymentZoneRadius > 0) {
        float radius = deploymentZoneRadius - inset;
        if (radius > 0) {
            position = ConstrainToCircle(position, deploymentZonePosition, radius);
        }
    }

    position = ConstrainToCircle(position, glm::vec2{512.0f, 512.0f}, 512.0f - 1.5f * inset);

    return position;
}


void CommandGesture::UpdateAdjustedPath(CommandGestureMarker* unitGestureMarker) {
    unitGestureMarker->adjustedPath.clear();
    unitGestureMarker->adjustedPath.insert(unitGestureMarker->adjustedPath.begin(), unitGestureMarker->path.begin(), unitGestureMarker->path.end());
}


static void RenderBounds(std::vector<glm::vec2>& vertices, bounds2f bounds) {
    vertices.push_back({bounds.min.x, bounds.min.y});
    vertices.push_back({bounds.max.x, bounds.min.y});
    vertices.push_back({bounds.max.x, bounds.min.y});
    vertices.push_back({bounds.max.x, bounds.max.y});
    vertices.push_back({bounds.max.x, bounds.max.y});
    vertices.push_back({bounds.min.x, bounds.max.y});
    vertices.push_back({bounds.min.x, bounds.max.y});
    vertices.push_back({bounds.min.x, bounds.min.y});
}


void CommandGesture::TryPlaySoundSample() {
    if (soundSampleId_ != SoundSampleID::Background) {
        soundDirector_->PlayUserInterfaceSound(soundSampleId_);
        soundSampleId_ = SoundSampleID::Background;
    }
}


void CommandGesture::RenderDebug2D() {
    std::vector<glm::vec2> vertices;

    for (auto unitId : unitController_->GetUnitIds()) {
        const auto& unitObject = unitController_->GetUnitObject(unitId);
        RenderBounds(vertices, unitController_->GetUnitBounds(unitObject["_position"_vec2]));
        if (unitController_->IsCommandableUnit(unitObject)) {
            RenderBounds(vertices, unitController_->GetUnitBounds(unitObject["_destination"_vec2]));
            RenderBounds(vertices, unitController_->GetUnitModifierBounds(unitId));
        }
    }

    auto array = Struct{} << "_" << Array();
    for (const auto& v : vertices)
        array = std::move(array) << v;

    debug2D_["vertices"] = *(std::move(array) << ValueEnd{} << ValueEnd{}).begin();
}



void CommandGesture::UpdateOffsetToMarker(bool finger) {
    if (auto pointer = GetCapturedPointer()) {
        float scaling = GetGestureSurface().GetViewport().getScaling();
        auto p1 = pointer->GetPreviousPosition();
        auto p2 = pointer->GetCurrentPosition();
        auto d = p2 - p1;

        offsetFactor_.x += glm::abs(d.x) / 48.0f / scaling;
        if (offsetFactor_.x > 1.0f)
            offsetFactor_.x = 1.0f;

        offsetFactor_.y += (d.y < 0 ? -d.y / 96.0f : d.y / 48.0f) / scaling;
        if (offsetFactor_.y > 1.0f)
            offsetFactor_.y = 1.0f;

        glm::vec2 offsetTarget{0.0f, finger ? 24.0f * scaling : 0.0f};
        offsetCurrent_ = glm::mix(offsetOriginal_, offsetTarget, offsetFactor_);
    }
}
