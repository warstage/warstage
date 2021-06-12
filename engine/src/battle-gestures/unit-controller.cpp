// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./unit-controller.h"
#include "./camera-control.h"
#include "./camera-gesture.h"
#include "./command-gesture.h"
#include "./editor-gesture.h"
#include "./editor-model.h"
#include "battle-simulator/convert-value.h"
#include "gesture/surface.h"


#define SNAP_TO_UNIT_TRESHOLD 22 // meters


UnitController::UnitController(Runtime* runtime, Surface* gestureSurface, Viewport& viewport, EditorObserver* editorObserver, SoundDirector* soundDirector) :
        strand_{Strand::getMain()},
        gestureSurface_{gestureSurface},
        viewport_{&viewport},
        runtime_{runtime},
        editorObserver_{editorObserver}
{
    cameraControl_ = new CameraControl{static_cast<bounds2f>(viewport.getViewportBounds()), viewport.getScaling()};

    battleFederate_ = std::make_shared<Federate>(*runtime, "Battle/BattleController", strand_);


    cameraObject_ = battleFederate_->getObjectClass("_Camera").create();

    UpdateCameraObject();
    UpdateRuntimeObjects();

    const int unitCommandGroup = 6;
    const int unitMissileGroup = 7;
    const int unitDeleteGroup = 8;

    systemFederate_ = std::make_shared<Federate>(*runtime, "System/BattleController", strand_);

    commandHalt_ = systemFederate_->getObjectClass("Command").create();
    commandHalt_["group"] = unitCommandGroup;
    commandHalt_["order"] = 1;
    commandHalt_["event"] = "unit-halt";
    commandHalt_["title"] = "Halt";
    commandHalt_["visible"] = true;
    commandHalt_["enabled"] = false;

    commandWalk_ = systemFederate_->getObjectClass("Command").create();
    commandWalk_["group"] = unitCommandGroup;
    commandWalk_["order"] = 2;
    commandWalk_["event"] = "unit-walk";
    commandWalk_["title"] = "Walk";
    commandWalk_["visible"] = true;
    commandWalk_["enabled"] = false;

    commandRun_ = systemFederate_->getObjectClass("Command").create();
    commandRun_["group"] = unitCommandGroup;
    commandRun_["order"] = 3;
    commandRun_["event"] = "unit-run";
    commandRun_["title"] = "Run";
    commandRun_["visible"] = true;
    commandRun_["enabled"] = false;

    commandHold_ = systemFederate_->getObjectClass("Command").create();
    commandHold_["group"] = unitMissileGroup;
    commandHold_["order"] = 4;
    commandHold_["event"] = "unit-hold";
    commandHold_["title"] = "Hold";
    commandHold_["visible"] = true;
    commandHold_["enabled"] = false;

    commandFire_ = systemFederate_->getObjectClass("Command").create();
    commandFire_["group"] = unitMissileGroup;
    commandFire_["order"] = 5;
    commandFire_["event"] = "unit-fire";
    commandFire_["title"] = "Fire";
    commandFire_["visible"] = true;
    commandFire_["enabled"] = false;

    commandDelete_ = systemFederate_->getObjectClass("Command").create();
    commandDelete_["group"] = unitDeleteGroup;
    commandDelete_["order"] = 1;
    commandDelete_["event"] = "unit-delete";
    commandDelete_["title"] = "X";
    commandDelete_["visible"] = false;
    commandDelete_["enabled"] = false;

    cameraGesture_ = std::make_shared<CameraGesture>(gestureSurface, this);
    commandGesture_ = std::make_shared<CommandGesture>(gestureSurface_, this, soundDirector);
}


UnitController::~UnitController() {
}


void UnitController::Startup(ObjectId battleFederationId, const std::string& playerId) {
    playerId_ = playerId;
    
    auto weak_ = weak_from_this();

  systemFederate_->getObjectClass("EditorModel").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock())
      this_->EditorModelChanged(object);
  });

  battleFederate_->getObjectClass("Terrain").observe([weak_](ObjectRef object) {
    if (auto this_ = weak_.lock()) {
      if (object.justDiscovered()) {
        this_->terrain_ = object;
      } else if (object.justDestroyed()) {
        this_->terrain_ = {};
      }
    }
  });

  battleFederate_->getEventClass("_SetMapCam").subscribe([weak_](const Value& event) {
    if (auto this_ = weak_.lock())
      this_->OnSetMapCam(event);
  });

  battleFederate_->getObjectClass("Unit").observe([weak_](ObjectRef unit) {
    if (auto this_ = weak_.lock()) {
      if (unit.justDiscovered()) {
        this_->OnAddUnit(unit.getObjectId());
      } else if (unit.justDestroyed()) {
        this_->OnRemoveUnit(unit.getObjectId());
      }
    }
  });
    
    /*_battleFederate->Objects("Commander").Observe([weak_, playerId](ObjectRef commander) {
        if (!commander.JustDestroyed()) {
            if (auto this_ = weak_.lock()) {
                if (const char* s = commander["playerId"_c_str]) {
                    if (playerId == s) {
                        this_->_allianceId = commander["alliance"_ObjectId];
                    }
                }
            }
        }
    });*/

  systemFederate_->getEventClass("unit-delete").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        auto unit = this_->battleFederate_->getObject(unitGestureMarker->unitId);
        unit["deletedByGesture"] = true;
      }
    }
  });

  systemFederate_->getEventClass("unit-halt").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        Value command{};
        auto unit = this_->battleFederate_->getObject(unitGestureMarker->unitId);
        auto path = DecodeArrayVec2(unit["_path"].getValue());
        if (path.size() >= 2) {
          auto delta = path[1] - path[0];
          if (glm::length(delta) >= 1) {
            command = Struct{}
                << "unit" << unitGestureMarker->unitId
                << "path" << Array{} << ValueEnd{}
                << "meleeTarget" << ObjectId{}
                << "facing" << angle(delta)
                << ValueEnd{};
          }
        }

        if (!command.has_value()) {
          command = Struct{}
              << "unit" << unitGestureMarker->unitId
              << "path" << Array{} << ValueEnd{}
              << "meleeTarget" << ObjectId{}
              << ValueEnd{};
        }
        this_->DispatchCommandEvent(command);

        unitGestureMarker->path.clear();
        unitGestureMarker->hasOrientation = false;
      }
    }
  });

  systemFederate_->getEventClass("unit-walk").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        if (auto unitObject = this_->GetUnitObject(unitGestureMarker->unitId)) {
          if (unitObject["running"_bool]) {
            this_->DispatchCommandEvent(Struct{}
                << "unit" << unitGestureMarker->unitId
                << "running" << false
                << ValueEnd{});
          }
        }
      }
    }
  });

  systemFederate_->getEventClass("unit-run").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        if (auto unitObject = this_->GetUnitObject(unitGestureMarker->unitId)) {
          if (!unitObject["running"_bool]) {
            this_->DispatchCommandEvent(Struct{}
                << "unit" << unitGestureMarker->unitId
                << "running" << true
                << ValueEnd{});
          }
        }
      }
    }
  });

  systemFederate_->getEventClass("unit-hold").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        if (auto unitObject = this_->GetUnitObject(unitGestureMarker->unitId)) {
          if (unitObject["stats.isMissile"_bool]) {
            this_->DispatchCommandEvent(Struct{}
                << "unit" << unitGestureMarker->unitId
                << "missileTarget" << unitGestureMarker->unitId
                << ValueEnd{});
          }
        }
      }
    }
  });

  systemFederate_->getEventClass("unit-fire").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock()) {
      for (auto unitGestureMarker : this_->unitGestureMarkers_) {
        if (auto unitObject = this_->GetUnitObject(unitGestureMarker->unitId)) {
          if (unitObject["stats.isMissile"_int]) {
            this_->DispatchCommandEvent(Struct{}
                << "unit" << unitGestureMarker->unitId
                << "missileTarget" << ObjectId{}
                << ValueEnd{});
          }
        }
      }
    }
  });

  systemFederate_->startup(Federation::SystemFederationId);
  battleFederate_->startup(battleFederationId);

    interval_ = strand_->setInterval([weak_]() {
      if (auto this_ = weak_.lock()) {
        this_->DeleteRoutedUnitGestureMarkers();
        this_->DeleteEmptyGestureGroups();
        this_->UpdateCommandButtons();
      }
    }, 200);

    commandGesture_->Initialize();
}


Promise<void> UnitController::shutdown_() {
    co_await *strand_;

  clearInterval(*interval_);

    commandHalt_.Delete();
    commandWalk_.Delete();
    commandRun_.Delete();
    commandHold_.Delete();
    commandFire_.Delete();
    commandDelete_.Delete();

    co_await battleFederate_->shutdown();
    co_await systemFederate_->shutdown();

    delete cameraControl_;
    cameraControl_ = nullptr;

    for (auto unitGestureMarker : unitGestureMarkers_) {
        delete unitGestureMarker;
    }
    unitGestureMarkers_.clear();
}


void UnitController::EditorModelChanged(ObjectRef object) {
    if (object.justDiscovered()) {
        editorModel_ = std::make_shared<EditorModel>(*runtime_, this, editorObserver_, object);
        editorModel_->Initialize();
        editorGesture_ = std::make_shared<EditorGesture>(gestureSurface_, this, editorModel_);
        CreateEditorCommands();

    } else if (object.justDestroyed()) {
        editorGesture_ = nullptr;
        editorModel_ = nullptr;
        DeleteEditorCommands();
    }

    UpdateCommandButtons();
}


void UnitController::CreateEditorCommands() {
    const int editorModeGroup = 10;
    const int terrainFeatureGroup = 11;
    
    commandEditorMode_[0] = systemFederate_->getObjectClass("Command").create();
    commandEditorMode_[0]["group"] = editorModeGroup;
    commandEditorMode_[0]["order"] = 1;
    commandEditorMode_[0]["event"] = "editor-hand";
    commandEditorMode_[0]["image"] = "images/editor-mode-hand.png";
    commandEditorMode_[0]["visible"] = true;
    commandEditorMode_[0]["enabled"] = false;
    
    commandEditorMode_[1] = systemFederate_->getObjectClass("Command").create();
    commandEditorMode_[1]["group"] = editorModeGroup;
    commandEditorMode_[1]["order"] = 2;
    commandEditorMode_[1]["event"] = "editor-paint";
    commandEditorMode_[1]["image"] = "images/editor-mode-paint.png";
    commandEditorMode_[1]["visible"] = true;
    commandEditorMode_[1]["enabled"] = false;
    
    commandEditorMode_[2] = systemFederate_->getObjectClass("Command").create();
    commandEditorMode_[2]["group"] = editorModeGroup;
    commandEditorMode_[2]["order"] = 3;
    commandEditorMode_[2]["event"] = "editor-erase";
    commandEditorMode_[2]["image"] = "images/editor-mode-erase.png";
    commandEditorMode_[2]["visible"] = true;
    commandEditorMode_[2]["enabled"] = false;
    
    commandEditorMode_[3] = systemFederate_->getObjectClass("Command").create();
    commandEditorMode_[3]["group"] = editorModeGroup;
    commandEditorMode_[3]["order"] = 4;
    commandEditorMode_[3]["event"] = "editor-smear";
    commandEditorMode_[3]["image"] = "images/editor-mode-smear.png";
    commandEditorMode_[3]["visible"] = true;
    commandEditorMode_[3]["enabled"] = false;
    
    
    commandTerrainFeature_[0] = systemFederate_->getObjectClass("Command").create();
    commandTerrainFeature_[0]["group"] = terrainFeatureGroup;
    commandTerrainFeature_[0]["order"] = 1;
    commandTerrainFeature_[0]["event"] = "editor-hills";
    commandTerrainFeature_[0]["image"] = "images/editor-feature-hills.png";
    commandTerrainFeature_[0]["visible"] = true;
    commandTerrainFeature_[0]["enabled"] = false;
    
    commandTerrainFeature_[1] = systemFederate_->getObjectClass("Command").create();
    commandTerrainFeature_[1]["group"] = terrainFeatureGroup;
    commandTerrainFeature_[1]["order"] = 3;
    commandTerrainFeature_[1]["event"] = "editor-water";
    commandTerrainFeature_[1]["image"] = "images/editor-feature-water.png";
    commandTerrainFeature_[1]["visible"] = true;
    commandTerrainFeature_[1]["enabled"] = false;
    
    commandTerrainFeature_[2] = systemFederate_->getObjectClass("Command").create();
    commandTerrainFeature_[2]["group"] = terrainFeatureGroup;
    commandTerrainFeature_[2]["order"] = 2;
    commandTerrainFeature_[2]["event"] = "editor-trees";
    commandTerrainFeature_[2]["image"] = "images/editor-feature-trees.png";
    commandTerrainFeature_[2]["visible"] = true;
    commandTerrainFeature_[2]["enabled"] = false;
    
    commandTerrainFeature_[3] = systemFederate_->getObjectClass("Command").create();
    commandTerrainFeature_[3]["group"] = terrainFeatureGroup;
    commandTerrainFeature_[3]["order"] = 4;
    commandTerrainFeature_[3]["event"] = "editor-fords";
    commandTerrainFeature_[3]["image"] = "images/editor-feature-fords.png";
    commandTerrainFeature_[3]["visible"] = true;
    commandTerrainFeature_[3]["enabled"] = false;
}


void UnitController::DeleteEditorCommands() {
    for (int i = 0; i < 4; ++i) {
        if (commandEditorMode_[i]) {
            commandEditorMode_[i].Delete();
            commandEditorMode_[i] = ObjectRef{};
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (commandTerrainFeature_[i]) {
            commandTerrainFeature_[i].Delete();
            commandTerrainFeature_[i] = ObjectRef{};
        }
    }
}


void UnitController::OnSetMap() {
    if (editorModel_)
        editorModel_->ResetUndoList();
}


void UnitController::OnSetMapCam(const Value& event) {
    if (acquireTerrainMap()) {
        int position = event["position"_int];
        GetCameraControl()->InitializeCameraPosition(position);
        UpdateCameraObject();
        UpdateRuntimeObjects();
    }
    releaseTerrainMap();
}


bool UnitController::acquireTerrainMap() {
    terrainMapMutex_.lock();
    if (terrain_) {
        terrainMap_ = terrain_.acquireShared<TerrainMap*>();
        if (terrainMap_)
            cameraControl_->SetHeightMap(&terrainMap_->getHeightMap(), false);
    }
    return terrainMap_ != nullptr;
}


void UnitController::releaseTerrainMap() {
    if (terrain_) {
      terrain_.releaseShared();
    }
    terrainMap_ = nullptr;
    if (cameraControl_) {
        cameraControl_->SetHeightMap(nullptr, false);
    }
    terrainMapMutex_.unlock();
}


void UnitController::UpdateCameraObject() {
    if (cameraObject_) {
        cameraObject_["value"] = Struct{}
            << "position" << cameraControl_->GetCameraPosition()
            << "facing" << cameraControl_->GetCameraFacing()
            << "tilt" << cameraControl_->GetCameraTilt()
            << ValueEnd{};
    }
}


void UnitController::UpdateRuntimeObjects() {
  battleFederate_->enterBlock_strand();
    UpdateUnitGestureMarkerObjects();
    UpdateUnitGestureGroupObjects();
  battleFederate_->leaveBlock_strand();
}


void UnitController::UpdateUnitGestureMarkerObjects() {
    for (auto unitGestureMarker : unitGestureMarkers_) {
        auto selectionMode = commandGesture_->unitGestureGroup_->selection;
        auto unitObject = GetUnitObject(unitGestureMarker->unitId);
        auto missileTarget = GetUnitObject(unitGestureMarker->missileTargetId);
        auto destination = !unitGestureMarker->adjustedPath.empty() ? unitGestureMarker->adjustedPath.back()
            : unitObject ? unitObject["_position"_vec2]
            : glm::vec2{};
        auto orientation = missileTarget ? missileTarget["_position"_vec2] : unitGestureMarker->orientationPoint;

        unitGestureMarker->object["unit"] = unitGestureMarker->unitId;
        unitGestureMarker->object["meleeTarget"] = unitGestureMarker->meleeTargetId;
        unitGestureMarker->object["missileTarget"] = unitGestureMarker->missileTargetId;
        unitGestureMarker->object["isPreliminary"] = unitGestureMarker->preliminaryAdded || unitGestureMarker->preliminaryRemoved;

       if (unitGestureMarker->renderSelected)
           unitGestureMarker->object["selectionMode"] = selectionMode ? true : false;
       else
           unitGestureMarker->object["selectionMode"] = selectionMode ? true : false; //nullptr;

        unitGestureMarker->object["running"] = unitGestureMarker->running;
        unitGestureMarker->object["facing"] = angle(orientation - destination);

        if (unitGestureMarker->hasOrientation)
            unitGestureMarker->object["orientation"] = unitGestureMarker->orientationPoint;
        else
            unitGestureMarker->object["orientation"] = nullptr;
        unitGestureMarker->object["renderOrientation"] = unitGestureMarker->renderOrientation;

        //unitGestureMarker->_object["path"] = unitGestureGroup->_movementMode != MovementMode::None ? unitGestureMarker->_adjustedPath : std::vector<glm::vec2>{};
        unitGestureMarker->object["path"] = unitGestureMarker->adjustedPath;
    }
}


void UnitController::UpdateUnitGestureGroupObjects() {
    if (commandGesture_ && commandGesture_->unitGestureGroup_) {
        if (commandGesture_->unitGestureGroup_->renderSelectionLasso) {
            commandGesture_->unitGestureGroup_->object["selectionAnchor"] = commandGesture_->unitGestureGroup_->selectionAnchor;
            commandGesture_->unitGestureGroup_->object["selectionPoint"] = commandGesture_->unitGestureGroup_->selectionPoint;
        } else {
            commandGesture_->unitGestureGroup_->object["selectionAnchor"] = nullptr;
            commandGesture_->unitGestureGroup_->object["selectionPoint"] = nullptr;
        }
    }
}


static void TrySetObjectProperty(ObjectRef& object, const char* property, bool value) {
    if (object && object[property].canSetValue() && object[property].getValue()._bool() != value) {
        object[property] = value;
    }
}


void UnitController::UpdateCommandButtons() {
    //int selectedCount = 0;
    //int missileCount = 0;
    //int haltingCount = 0;
    //int walkingCount = 0;
    //int runningCount = 0;
    //int holdingCount = 0;
    //int firingCount = 0;
    bool enableDelete = false;
    bool enableHalt = false;
    bool enableWalk = false;
    bool enableRun = false;
    bool enableHold = false;
    bool enableFire = false;

    for (auto unitGestureMarker : unitGestureMarkers_) {
        if (!unitGestureMarker->preliminaryAdded) {
            if (auto unitObject = GetUnitObject(unitGestureMarker->unitId)) {
                //++selectedCount;
                //if (unit->isRunning)
                //    ++runningCount;
                //else if (unit->isMoving)
                //    ++walkingCount;
                //else
                //    ++haltingCount;

                if (unitObject["deletable"_bool])
                    enableDelete = true;

                if (unitObject["running"_bool] || unitObject["_moving"_bool])
                    enableHalt = true;
                if (unitObject["running"_bool])
                    enableWalk = true;
                else if (unitObject["_moving"_bool])
                    enableRun = true;

                if (unitObject["stats.isMissile"_bool]) {
                    //++missileCount;
                    if (unitObject["missileTarget"_ObjectId])
                        enableFire = true;
                    if (unitObject["missileTarget"_ObjectId] != unitGestureMarker->unitId)
                        enableHold = true;
                }
            }
        }
    }

    TrySetObjectProperty(commandHalt_, "enabled", enableHalt);
    TrySetObjectProperty(commandWalk_, "enabled", enableWalk);
    TrySetObjectProperty(commandRun_, "enabled", enableRun);
    TrySetObjectProperty(commandHold_, "enabled", enableHold);
    TrySetObjectProperty(commandFire_, "enabled", enableFire);
    TrySetObjectProperty(commandDelete_, "enabled", enableDelete);

    bool showEditor = editorModel_ != nullptr
        && !enableDelete
        && !enableHalt
        && !enableWalk
        && !enableRun
        && !enableHold
        && !enableFire;
    bool showCommands = !showEditor;

    TrySetObjectProperty(commandHalt_, "visible", showCommands);
    TrySetObjectProperty(commandWalk_, "visible", showCommands);
    TrySetObjectProperty(commandRun_, "visible", showCommands);
    TrySetObjectProperty(commandHold_, "visible", showCommands);
    TrySetObjectProperty(commandFire_, "visible", showCommands);
    TrySetObjectProperty(commandDelete_, "visible", showCommands && enableDelete);

    if (editorModel_) {
        for (int i = 0; i < 4; ++i) {
            TrySetObjectProperty(commandEditorMode_[i], "visible", showEditor);
        }
        for (int i = 0; i < 4; ++i) {
            TrySetObjectProperty(commandTerrainFeature_[i], "visible", showEditor);
        }
    }

    if (showEditor) {
        int editorMode = static_cast<int>(editorModel_->GetEditorMode());
        for (int i = 0; i < 4; ++i) {
            bool selected = editorMode == i;
            TrySetObjectProperty(commandEditorMode_[i], "selected", selected);
            bool enabled = editorMode != i;
            TrySetObjectProperty(commandEditorMode_[i], "enabled", enabled);
        }
        int terrainFeature = static_cast<int>(editorModel_->GetTerrainFeature());
        for (int i = 0; i < 4; ++i) {
            bool selected = terrainFeature == i;
            TrySetObjectProperty(commandTerrainFeature_[i], "selected", selected);
            bool enabled = terrainFeature != i;
            TrySetObjectProperty(commandTerrainFeature_[i], "enabled", enabled);
        }
    }
}


ObjectRef UnitController::GetUnitObject(ObjectId unitId) const {
    return battleFederate_->getObject(unitId);
}


std::vector<ObjectRef> UnitController::GetCommanders(ObjectId allianceId) {
    std::vector<ObjectRef> result;
    for (auto&& commanderObject : battleFederate_->getObjectClass("Commander"))
        if (commanderObject["alliance"_ObjectId] == allianceId)
            result.push_back(commanderObject);
    return result;
}


std::unique_ptr<UnitGestureGroup> UnitController::MakeUnitGestureGroup() {
    auto unitGestureGroup = std::make_unique<UnitGestureGroup>();
    unitGestureGroup->object = battleFederate_->getObjectClass("_UnitGestureGroup").create();
    return unitGestureGroup;
}


bool UnitController::DeleteEmptyGestureGroups() {
    bool result = false;
    /*auto i = _unitGestureGroups.begin();
    while (i != _unitGestureGroups.end()) {
        if ((*i)->_unitGestureMarkers.empty()) {
            (*i)->_object.Delete();
            delete *i;
            i = _unitGestureGroups.erase(i);
            result = true;
        } else {
            ++i;
        }
    }*/
    return result;
}


CommandGestureMarker* UnitController::AddUnitGestureMarker(ObjectId unitId) {
    auto unitGestureMarker = new CommandGestureMarker(unitId);
    unitGestureMarker->object = battleFederate_->getObjectClass("_UnitGestureMarker").create();
    unitGestureMarkers_.push_back(unitGestureMarker);
    commandGesture_->unitGestureGroup_->unitGestureMarkers.push_back(unitGestureMarker);
    return unitGestureMarker;
}


CommandGestureMarker* UnitController::FindUnitGestureMarker(ObjectId unitId) const {
    for (auto unitGestureMarker : unitGestureMarkers_)
        if (unitGestureMarker->unitId == unitId)
            return unitGestureMarker;
    return nullptr;
}


void UnitController::DeleteUnitGestureMarker(CommandGestureMarker* unitGestureMarker) {
    if (commandGesture_->unitGestureMarker_ && commandGesture_->unitGestureMarker_ == unitGestureMarker) {
        commandGesture_->unitGestureMarker_ = nullptr;
        commandGesture_->unitGestureState_ = UnitGestureState::None;
    }

    auto& selectedUnits = commandGesture_->unitGestureGroup_->unitGestureMarkers;
    selectedUnits.erase(
        std::remove(selectedUnits.begin(), selectedUnits.end(), unitGestureMarker),
        selectedUnits.end());
    if (selectedUnits.empty()) {
        commandGesture_->unitGestureGroup_->selection = false;
    }
    unitGestureMarkers_.erase(
        std::remove(unitGestureMarkers_.begin(), unitGestureMarkers_.end(), unitGestureMarker),
        unitGestureMarkers_.end());

    unitGestureMarker->object.Delete();
    delete unitGestureMarker;
}


void UnitController::DeleteRoutedUnitGestureMarkers() {
    std::vector<CommandGestureMarker*> routed{};
    for (auto unitGestureMarker : unitGestureMarkers_) {
        if (auto unitObject = GetUnitObject(unitGestureMarker->unitId)) {
            if (unitObject["_routing"_bool]) {
                routed.push_back(unitGestureMarker);
            }
        }
    }
    for (auto unitGestureMarker : routed)
        DeleteUnitGestureMarker(unitGestureMarker);
}


ObjectId UnitController::FindCommandableUnit(glm::vec2 screenPosition, glm::vec2 terrainPosition) {
    auto unitByPosition = FindCommandableUnitByPosition(screenPosition, terrainPosition);
    auto unitByDestination = FindCommandableUnitByDestination(screenPosition, terrainPosition);
    auto unitByOrientation = FindCommandableUnitByOrientation(screenPosition, terrainPosition);

    if (unitByPosition && !unitByDestination) {
        return unitByPosition;
    }

    if (!unitByPosition && unitByDestination) {
        return unitByDestination;
    }

    auto unitByPositionState = GetUnitObject(unitByPosition);
    auto unitByDestinationState = GetUnitObject(unitByDestination);
    if (unitByPositionState && unitByDestinationState) {
        float distanceToPosition = glm::distance(unitByPositionState["_position"_vec2], screenPosition);
        float distanceToDestination = glm::distance(unitByDestinationState["_destination"_vec2], screenPosition);
        return distanceToPosition < distanceToDestination + 24
            ? unitByPosition
            : unitByDestination;
    }

    if (unitByOrientation)
        return unitByOrientation;

    return ObjectId{};
}


ObjectId UnitController::FindCommandableUnitByPosition(glm::vec2 screenPosition, glm::vec2 terrainPosition) {
    ObjectId result{};
    if (auto unitId = GetNearestUnitByPosition(terrainPosition, ObjectId{}, true)) {
        if (auto unitObject = GetUnitObject(unitId)) {
            if (!unitObject["_routing"_bool] && GetUnitBounds(unitObject["_position"_vec2]).contains(screenPosition)) {
                result = unitId;
            }
        }
    }
    return result;
}


ObjectId UnitController::FindCommandableUnitByDestination(glm::vec2 screenPosition, glm::vec2 terrainPosition) {
    ObjectId result{};
    if (auto unitId = GetNearestUnitByDestination(terrainPosition, true)) {
        if (auto unitObject = GetUnitObject(unitId)) {
            if (!unitObject["_routing"_bool] && GetUnitBounds(unitObject["_destination"_vec2]).contains(screenPosition)) {
                result = unitId;
            }
        }
    }
    return result;
}


ObjectId UnitController::FindCommandableUnitByOrientation(glm::vec2 screenPosition, glm::vec2 terrainPosition) {
    ObjectId result{};
    float distance = 10000;

    for (ObjectId unitId : GetUnitIds()) {
        if (auto unitObject = GetUnitObject(unitId)) {
            if (IsCommandableUnit(unitObject)) {
                glm::vec2 center = unitObject["_destination"_vec2];
                float d = glm::distance(center, terrainPosition);
                if (d < distance && !unitObject["_routing"_bool] && GetUnitModifierBounds(unitId).contains(screenPosition)) {
                    result = unitId;
                    distance = d;
                }
            }
        }
    }

    return result;
}


void UnitController::FindCommandableUnitGroup(std::vector<ObjectId>& result, const std::vector<ObjectId>& unitIds, glm::vec2 position) {
    for (auto unitId : unitIds) {
        if (std::find(result.begin(), result.end(), unitId) == result.end()) {
            if (auto unitObject = GetUnitObject(unitId)) {
                if (IsCommandableUnit(unitObject) && glm::distance(unitObject["_position"_vec2], position) < 64) {
                    result.push_back(unitId);
                    FindCommandableUnitGroup(result, unitIds, unitObject["_position"_vec2]);
                }
            }
        }
    }
}


ObjectId UnitController::FindEnemyUnit(glm::vec2 pointerPosition, glm::vec2 markerPosition) {
    ObjectId enemyUnit{};
    if (auto unitObject = GetUnitObject(commandGesture_->unitGestureMarker_->unitId)) {
        auto filterNotAllianceId = unitObject["alliance"_ObjectId];

        auto p = markerPosition;
        auto d = (pointerPosition - markerPosition) / 4.0f;
        for (int i = 0; i < 4; ++i) {
            if (auto unitId = GetNearestUnitByPosition(p, filterNotAllianceId, false)) {
                if (auto enemyObject = GetUnitObject(unitId)) {
                    if (glm::distance(enemyObject["_position"_vec2], p) <= SNAP_TO_UNIT_TRESHOLD) {
                        enemyUnit = unitId;
                        break;
                    }
                }
            }
            p += d;
        }
    }
    return enemyUnit;
}


bounds2f UnitController::GetUnitBounds(glm::vec2 center) {
    return GetUnitIconViewportBounds(center).add_radius(12);
}


bounds2f UnitController::GetUnitModifierBounds(ObjectId unitId) {
    if (auto unitObject = GetUnitObject(unitId)) {
        if (unitObject["_standing"_bool])
            return GetCameraControl()->GetUnitFacingMarkerBounds(unitObject["_position"_vec2], unitObject["facing"_float]).add_radius(12);

        if (unitObject["_moving"_bool]) {
            return GetCameraControl()->GetUnitFacingMarkerBounds(unitObject["_destination"_vec2], unitObject["facing"_float]).add_radius(12);
        }
    }

    return bounds2f();
}


bounds2f UnitController::GetUnitIconViewportBounds(glm::vec2 center) {
    auto position = getHeightMap().getPosition(center, 0);
    return GetCameraControl()->GetUnitMarkerBounds(position);
}


ObjectId UnitController::GetNearestUnitByPosition(glm::vec2 position, ObjectId filterNotAllianceId, bool filterCommandable) {
    ObjectId result{};
    float nearest = INFINITY;

    for (auto unitId : GetUnitIds()) {
        if (auto unitObject = GetUnitObject(unitId)) {
            if (filterNotAllianceId && unitObject["alliance"_ObjectId] == filterNotAllianceId)
                continue;
            if (filterCommandable && !IsCommandableUnit(unitObject))
                continue;

            glm::vec2 p = unitObject["_position"_vec2];
            float dx = p.x - position.x;
            float dy = p.y - position.y;
            float d = dx * dx + dy * dy;
            if (d < nearest) {
                result = unitId;
                nearest = d;
            }
        }
    }

    return result;
}


ObjectId UnitController::GetNearestUnitByDestination(glm::vec2 position, bool filterCommandable) {
    ObjectId result{};
    float nearest = INFINITY;

    for (auto unitId : GetUnitIds()) {
        if (auto unitObject = GetUnitObject(unitId)) {
            if (filterCommandable && !IsCommandableUnit(unitObject))
                continue;
        
            glm::vec2 p = unitObject["_destination"_vec2];
            float dx = p.x - position.x;
            float dy = p.y - position.y;
            float d = dx * dx + dy * dy;
            if (d < nearest) {
                result = unitId;
                nearest = d;
            }
        }
    }

    return result;
}


ObjectRef UnitController::GetNearestDeploymentUnit(glm::vec2 screenPosition) {
    ObjectRef result{};
    float nearest = 0;
    
    for (const auto& deploymentUnit : battleFederate_->getObjectClass("DeploymentUnit")) {
        if (IsCommandableDeploymentUnit(deploymentUnit)) {
            auto bounds = GetUnitBounds(deploymentUnit["position"_vec2]);
            if (bounds.contains(screenPosition)) {
                auto p = bounds.mid();
                float dx = p.x - screenPosition.x;
                float dy = p.y - screenPosition.y;
                float d = dx * dx + dy * dy;
                if (!result || d < nearest) {
                    result = deploymentUnit;
                    nearest = d;
                }
            }
        }
    }
    return result;
}


glm::vec2 UnitController::ConstrainToContent(glm::vec2 position) const {
    auto contentBounds = getHeightMap().getBounds();
    auto contentCenter = contentBounds.mid();
    float contentRadius = contentBounds.x().size() / 2;
    float maximumRadius = contentRadius - 25.0f;
    auto markerOffsetFromCenter = position - contentCenter;
    if (glm::length(markerOffsetFromCenter) > maximumRadius)
        position = contentCenter + glm::normalize(markerOffsetFromCenter) * maximumRadius;
    return position;
}


glm::vec2 UnitController::ConstrainToDeploymentZone(glm::vec2 position, ObjectId deploymentZoneId) const {
    auto deploymentZone = battleFederate_->getObject(deploymentZoneId);
    if (!deploymentZone) {
        return position;
    }
    auto center = deploymentZone["position"_vec2];
    float radius = deploymentZone["radius"_float];
    if (radius < 1.0f) {
        return center;
    }
    float distance = glm::distance(center, position);
    if (distance <= radius) {
        return position;
    }
    return center + radius * (position - center) / distance;
}


glm::vec2 UnitController::ConstrainImpassable(glm::vec2 currentDestination, glm::vec2 markerPosition) const {
    float movementLimit = -1.0f;
    float delta = 1.0f / glm::max(1.0f, glm::distance(currentDestination, markerPosition));
    float k = delta;
    while (k < 1.0f) {
        auto& terrainMap = getTerrainMap();
        if (terrainMap.isImpassable(glm::mix(currentDestination, markerPosition, k))) {
            movementLimit = k;
            break;
        }
        k += delta;
    }

    if (movementLimit >= 0) {
        auto diff = markerPosition - currentDestination;
        markerPosition = currentDestination + diff * movementLimit;
        markerPosition -= glm::normalize(diff) * 10.0f;
    }

    return markerPosition;
}


void UnitController::DispatchCommandEvent(const Value& value) {
  battleFederate_->getEventClass("_Commander").dispatch(Struct{}
      << "playerId" << playerId_
      << ValueEnd{});
  battleFederate_->getEventClass("Command").dispatch(value);
}


bool UnitController::IsCommandableUnit(const ObjectRef& unit) const {
    if (auto commander = battleFederate_->getObject(unit["commander"_ObjectId])) {
        if (const char* playerId = commander["playerId"_c_str]) {
            if (playerId_ == playerId)
                return true;
        }
    }
    return unit["delegated"_bool] && IsPlayerAlliance(unit["alliance"_ObjectId]);
}


bool UnitController::IsCommandableDeploymentUnit(const ObjectRef& deploymentUnit) const {
    if (const char* hostingPlayerId = deploymentUnit["hostingPlayerId"_c_str]) {
        return playerId_ == hostingPlayerId;
    }
    if (auto commander = battleFederate_->getObject(deploymentUnit["commander"_ObjectId])) {
        if (const char* playerId = commander["playerId"_c_str]) {
            if (playerId_ == playerId)
                return true;
        }
    }
    return IsPlayerAlliance(deploymentUnit["alliance"_ObjectId]);
}


bool UnitController::IsPlayerAlliance(ObjectId allianceId) const {
    for (const auto& commander : battleFederate_->getObjectClass("Commander")) {
        if (const char* playerId = commander["playerId"_c_str]) {
            if (playerId_ == playerId && commander["alliance"_ObjectId] == allianceId) {
                return true;
            }
        }
    }
    return false;
}


void UnitController::OnAddUnit(ObjectId unitId) {
    unitIds_.push_back(unitId);
}


void UnitController::OnRemoveUnit(ObjectId unitId) {
    for (auto i = unitGestureMarkers_.begin(); i != unitGestureMarkers_.end(); ++i) {
        auto unitGestureMarker = *i;
        if (unitGestureMarker->meleeTargetId == unitId)
            unitGestureMarker->meleeTargetId = ObjectId{};
        if (unitGestureMarker->missileTargetId == unitId)
            unitGestureMarker->missileTargetId = ObjectId{};
    }
    unitIds_.erase(
        std::remove(unitIds_.begin(), unitIds_.end(), unitId),
        unitIds_.end());

    if (auto unitGestureMarker = FindUnitGestureMarker(unitId)) {
        DeleteUnitGestureMarker(unitGestureMarker);
    }
}
