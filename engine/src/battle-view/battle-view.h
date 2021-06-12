// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__BATTLE_VIEW_H
#define WARSTAGE__BATTLE_VIEW__BATTLE_VIEW_H

#include "./camera-state.h"
#include "./battle-animator.h"
#include "./shaders.h"
#include "./render-body.h"
#include "./render-terrain.h"
#include "battle-simulator/convert-value.h"
#include "battle-model/battle-vm.h"
#include "runtime/runtime.h"

class BackgroundView;
class TerrainMap;
class DebugRenderer;
class FacingRenderer;
class Surface;
class MarkerRenderer;
class MovementRenderer;
class PhantomRenderer;
class Pointer;
class RangeRenderer;
class SelectionRenderer;
class SkyRenderer;
class UnitController;
class WaterRenderer;


class BattleView :
        public Shutdownable,
        public std::enable_shared_from_this<BattleView>
{
public:
    Viewport* viewport_;
    Graphics* graphics_{};
    std::shared_ptr<SoundDirector> soundDirector_{};

    std::string playerId_{};
    ObjectId commanderId_{};
    ObjectId commanderAllianceId_{};
    ObjectId defaultAllianceId_{};

    // ViewState

    BattleVM::Model viewModel_{};
    std::shared_ptr<CameraState> cameraState_;
    bounds2f treesDirty_{};

    std::shared_ptr<Federate> battleFederate_{};
    ObjectRef battleStatistics_;
    ObjectRef cameraObject_{};
    ObjectRef terrain_{};

    // Renderers

    SkyRenderer* renderSky_{};
    TerrainRenderer* renderTerrain_{};
    WaterRenderer* renderWater_{};
    RangeRenderer* renderRange_{};
    FacingRenderer* renderFacing_{};
    MarkerRenderer* renderMarker_{};
    MovementRenderer* renderMovement_{};
    PhantomRenderer* renderPhantom_{};
    SelectionRenderer* renderSelection_{};
    BodyRenderer renderBody_;
    DebugRenderer* renderDebug_{};

    BattleAnimator battleAnimator_;

public:
    BattleView(Runtime& runtime, Viewport& viewport, std::shared_ptr<SoundDirector> soundDirector);
    ~BattleView() override;

    ObjectId getFederationId() const;

    void startup(ObjectId battleFederationId, const std::string& playerId);

protected: // Shutdownable
    [[nodiscard]] Promise<void> shutdown_() override;

public:
    [[nodiscard]] Graphics* getGraphics() const { return graphics_; }
    [[nodiscard]] CameraState& getCameraState() const { return *cameraState_; }

    [[nodiscard]] const TerrainMap& getTerrainMap() const;
    [[nodiscard]] const HeightMap& getHeightMap() const;

    [[nodiscard]] Viewport& getViewport();

//private:
    void handleUnitDiscovered_(ObjectRef object);
    void handleUnitDestroyed_(ObjectId objectId);
    void handleUnitChanged_(BattleVM::Unit& unitVM);
    BattleVM::Marker toMarker_(const Value& value);
    BattleVM::MarkerLayer toMarkerLayer_(const Value& value, float texgrid);
    void updateUnitElements_();

    void handleTerrainChanged_(ObjectRef terrain);
    void updateTerrain_();
    void setTerrainDirty(TerrainFeature terrainFeature, bounds2f bounds);
    void setTreesDirty_(bounds2f bounds);


    bool acquireTerrainMap_();
    void releaseTerrainMap_();

    void discoverShape_(const ObjectRef& object);
    BattleVM::Line toLine_(const Value& value);
    glm::vec4 toColor_(const Value& value);
    BattleVM::Skin toSkin_(const Value& value);
    BattleVM::Loop toLoop_(const Value& value);
    std::vector<float> toFloatVector_(const Value& value, float scale = 1.0f);
    BattleVM::LoopType toLoopType_(const Value& value);
    void setLoopType_(BattleVM::LoopType& result, bool cond, BattleVM::LoopType flag);

    void acquireBattleControllerFrame_();

    const std::vector<RootPtr<BattleVM::Unit>>& getUnits_() const { return viewModel_.units; }

    const std::string& getPlayerId_() const { return playerId_; }
    ObjectId getCommanderId_() const { return commanderId_; }
    ObjectId getAllianceId_() const { return commanderAllianceId_ ?: defaultAllianceId_; }

    void addCasualty_(ObjectId unitId, glm::vec2 position);

    void render_(Framebuffer* frameBuffer, BackgroundView* backgroundView);

    void animate_(double secondsSinceLastUpdate);

    BattleVM::Unit* getUnitVM_(ObjectId unitId) const;

    void updateSoundPlayer_();

    ObjectRef getAlliance_(ObjectId allianceId) const;
    int getAlliancePosition_(ObjectId allianceId) const;

    bool isCommandable_(const ObjectRef& unit) const;
    bool isPlayerAlliance_(ObjectId allianceId) const;
    bool shouldShowMovementPath_(const ObjectRef& unit) const;
};


#endif
