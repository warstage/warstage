// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__UNIT_CONTROLLER_H
#define WARSTAGE__BATTLE_GESTURES__UNIT_CONTROLLER_H

#include "./command-gesture.h"
#include "runtime/runtime.h"
#include <memory>

class CameraControl;
class EditorGesture;
class EditorModel;
class EditorObserver;
class TerrainMap;
class SoundDirector;
class Surface;
class CameraGesture;
struct CommandGestureMarker;
struct UnitGestureGroup;
class Viewport;



class UnitController :
        public Shutdownable,
        public std::enable_shared_from_this<UnitController>
{
    std::shared_ptr<Strand> strand_{};
    Surface* gestureSurface_{};
    Viewport* viewport_{};
    std::mutex terrainMapMutex_{};
    TerrainMap* terrainMap_{};
    CameraControl* cameraControl_{};
    std::shared_ptr<IntervalObject> interval_{};

    std::vector<ObjectId> unitIds_{};
    std::vector<CommandGestureMarker*> unitGestureMarkers_{};

    std::shared_ptr<CameraGesture> cameraGesture_{};
    std::shared_ptr<CommandGesture> commandGesture_{};
    
    std::string playerId_{};
    // ObjectId allianceId_{};

public:
    std::shared_ptr<Federate> battleFederate_{};
    ObjectRef terrain_{};
    ObjectRef cameraObject_{};

    std::shared_ptr<EditorModel> editorModel_{};
    std::shared_ptr<EditorGesture> editorGesture_{};

    std::shared_ptr<Federate> systemFederate_{};
    ObjectRef commandDelete_{};
    ObjectRef commandHalt_{};
    ObjectRef commandWalk_{};
    ObjectRef commandRun_{};
    ObjectRef commandHold_{};
    ObjectRef commandFire_{};

    ObjectRef commandEditorMode_[4]{};
    ObjectRef commandTerrainFeature_[4]{};

    Runtime* runtime_{};
    EditorObserver* editorObserver_{};

public:
    UnitController(Runtime* runtime, Surface* gestureSurface, Viewport& viewport, EditorObserver* editorObserver, SoundDirector* soundDirector);
    ~UnitController() override;

    void Startup(ObjectId battleFederationId, const std::string& playerId);

protected: // Shutdownable
    [[nodiscard]] Promise<void> shutdown_() override;

public:
    void EditorModelChanged(ObjectRef object);
    void CreateEditorCommands();
    void DeleteEditorCommands();
    
    void OnSetMap();
    void OnSetMapCam(const Value& event);

    bool acquireTerrainMap();
    void releaseTerrainMap();

    const TerrainMap& getTerrainMap() const { return *terrainMap_; }
    const HeightMap& getHeightMap() const { return terrainMap_->getHeightMap(); }

    void UpdateCameraObject();
    void UpdateRuntimeObjects();
    void UpdateUnitGestureMarkerObjects();
    void UpdateUnitGestureGroupObjects();
    void UpdateCommandButtons();

    CameraControl* GetCameraControl() const { return cameraControl_; }

    const std::vector<ObjectId>& GetUnitIds() const { return unitIds_; }

    ObjectRef GetUnitObject(ObjectId unitId) const;
    std::vector<ObjectRef> GetCommanders(ObjectId allianceId);

    std::unique_ptr<UnitGestureGroup> MakeUnitGestureGroup();
    bool DeleteEmptyGestureGroups();

    const std::vector<CommandGestureMarker*>& GetUnitGestureMarkers() const { return unitGestureMarkers_; }
    CommandGestureMarker* FindUnitGestureMarker(ObjectId unitId) const;
    CommandGestureMarker* AddUnitGestureMarker(ObjectId unitId);
    void DeleteUnitGestureMarker(CommandGestureMarker* unitGestureMarker);
    void DeleteRoutedUnitGestureMarkers();

    ObjectId FindCommandableUnit(glm::vec2 screenPosition, glm::vec2 terrainPosition);
    ObjectId FindCommandableUnitByPosition(glm::vec2 screenPosition, glm::vec2 terrainPosition);
    ObjectId FindCommandableUnitByDestination(glm::vec2 screenPosition, glm::vec2 terrainPosition);
    ObjectId FindCommandableUnitByOrientation(glm::vec2 screenPosition, glm::vec2 terrainPosition);
    void FindCommandableUnitGroup(std::vector<ObjectId>& result, const std::vector<ObjectId>& unitIds, glm::vec2 position);
    ObjectId FindEnemyUnit(glm::vec2 pointerPosition, glm::vec2 markerPosition);

    bounds2f GetUnitBounds(glm::vec2 center);
    bounds2f GetUnitModifierBounds(ObjectId unitId);
    bounds2f GetUnitIconViewportBounds(glm::vec2 center);

    ObjectId GetNearestUnitByPosition(glm::vec2 position, ObjectId filterNotAllianceId, bool filterCommandable);
    ObjectId GetNearestUnitByDestination(glm::vec2 position, bool filterCommandable);
    ObjectRef GetNearestDeploymentUnit(glm::vec2 position);

    glm::vec2 ConstrainToContent(glm::vec2 position) const;
    glm::vec2 ConstrainToDeploymentZone(glm::vec2 position, ObjectId deploymentZoneId) const;
    glm::vec2 ConstrainImpassable(glm::vec2 currentDestination, glm::vec2 markerPosition) const;

    void DispatchCommandEvent(const Value& value);
    
    bool IsCommandableUnit(const ObjectRef& unit) const;
    bool IsCommandableDeploymentUnit(const ObjectRef& deploymentUnit) const;
    bool IsPlayerAlliance(ObjectId allianceId) const;

private:
    void OnAddUnit(ObjectId unitId);
    void OnRemoveUnit(ObjectId unitId);
};


#endif
