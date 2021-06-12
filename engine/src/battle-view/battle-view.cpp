// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./battle-view.h"
#include "./camera-state.h"
#include "./render-background.h"
#include "battle-audio/sound-director.h"
#include "battle-gestures/unit-controller.h"
#include "battle-simulator/convert-value.h"
#include "render-facing.h"
#include "render-marker.h"
#include "render-debug.h"
#include "render-movement.h"
#include "render-phantom.h"
#include "render-range.h"
#include "render-selection.h"
#include "render-sky.h"
#include "render-water.h"
#include <glm/gtc/constants.hpp>
#include <iostream>


namespace {
    constexpr float tex(const int v) {
        return 0.125f * static_cast<float>(v);
    }

    void AddLoop(BattleVM::Skin& skin, const BattleVM::LoopType type, std::vector<float> angles, std::vector<float> vertices, const float scale = 1.0f) {
        skin.loops.push_back({
                .type = type,
                .texture = 0,
                .angles = std::move(angles),
                .vertices = std::move(vertices),
                .duration = 0.0f,
                .repeat = false
        });
    }
}


BattleView::BattleView(Runtime& runtime, Viewport& viewport, std::shared_ptr<SoundDirector> soundDirector) :
        viewport_{&viewport},
        graphics_{viewport.getGraphics()},
        soundDirector_{soundDirector},
        cameraState_{std::make_shared<CameraState>(static_cast<bounds2f>(viewport.getViewportBounds()), viewport.getScaling())},
        battleAnimator_{viewModel_, soundDirector},
        renderBody_{*viewport.getGraphics()}
{
    renderSky_ = new SkyRenderer(*graphics_);
    renderRange_ = new RangeRenderer(*graphics_);
    renderFacing_ = new FacingRenderer(*graphics_);
    renderMarker_ = new MarkerRenderer(*graphics_);
    renderMovement_ = new MovementRenderer(*graphics_);
    renderPhantom_ = new PhantomRenderer(*graphics_);
    renderDebug_ = new DebugRenderer(*graphics_);
    renderSelection_ = new SelectionRenderer(*graphics_);
    renderTerrain_ = new TerrainRenderer(this);
    renderWater_ = new WaterRenderer(*graphics_);

    battleFederate_ = std::make_shared<Federate>(runtime, "Battle/BattleView", Strand::getRender());
}


BattleView::~BattleView() {
    LOG_ASSERT(battleFederate_->shutdownCompleted());
}


ObjectId BattleView::getFederationId() const {
    return battleFederate_ ? battleFederate_->getFederationId() : ObjectId{};
}


void BattleView::startup(ObjectId battleFederationId, const std::string& playerId) {
    playerId_ = playerId;

    auto weak_ = weak_from_this();

    battleFederate_->getObjectClass("_BattleStatistics").observe([weak_](ObjectRef object) {
        if (auto this_ = weak_.lock()) {
            this_->battleStatistics_ = object;
        }
    });

    battleFederate_->getObjectClass("Commander").observe([weak_, playerId](ObjectRef commander) {
        if (!commander.justDestroyed()) {
            if (auto this_ = weak_.lock()) {
                if (const char* s = commander["playerId"_c_str]) {
                    if (playerId == s) {
                        this_->commanderId_ = commander.getObjectId();
                        this_->commanderAllianceId_ = commander["alliance"_ObjectId];
                        if (this_->commanderAllianceId_) {
                            this_->defaultAllianceId_ = this_->commanderAllianceId_;
                        }
                    }
                }
            }
        }
    });

    battleFederate_->getObjectClass("Alliance").observe([weak_, playerId](ObjectRef object) {
        if (!object.justDestroyed()) {
            if (auto this_ = weak_.lock()) {
                if (!this_->defaultAllianceId_) {
                    this_->defaultAllianceId_ = object.getObjectId();
                }
            }
        }
    });

    battleFederate_->getObjectClass("Unit").require({"commander", "alliance", "unitType", "marker"});
    battleFederate_->getObjectClass("Unit").observe([weak_](ObjectRef object) {
        if (auto this_ = weak_.lock()) {
            if (object.justDiscovered()) {
                this_->handleUnitDiscovered_(object);
            } else if (object.justDestroyed()) {
                this_->handleUnitDestroyed_(object.getObjectId());
            } else if (auto* unitVM = this_->getUnitVM_(object.getObjectId())) {
                this_->handleUnitChanged_(*unitVM);
            } else {
                LOG_ASSERT(this_->getUnitVM_(object.getObjectId()));
            }
        }
    });

    battleFederate_->getEventClass("FighterCasualty").subscribe([weak_](const Value& event) {
        if (auto this_ = weak_.lock()) {
            if (this_->acquireTerrainMap_()) {
                this_->addCasualty_(event["unit"_ObjectId], event["fighter"_vec2]);
            }
            this_->releaseTerrainMap_();

            Strand::getMain()->setImmediate([soundDirector = this_->soundDirector_]() {
                soundDirector->PlayCasualty();
            });
        }
    });

    battleFederate_->getEventClass("MissileRelease").subscribe([weak_](const Value& event) {
        if (auto this_ = weak_.lock()) {
            if (this_->acquireTerrainMap_()) {
                if (const auto unit = this_->viewModel_.FindUnit(event["unit"_ObjectId])) {
                    if (const auto* missileStats = unit->FindMissileStats(event["missileType"_int])) {
                        auto projectiles = ProjectileFromBson(event["projectiles"]);
                        float timeToImpact = event["timeToImpact"_float];
                        this_->battleAnimator_.AddVolleyAndProjectiles(*missileStats, projectiles, timeToImpact);
                    }
                }
            }
            this_->releaseTerrainMap_();
        }
    });

    battleFederate_->getObjectClass("_Camera").observe([weak_](ObjectRef object) {
        if (auto this_ = weak_.lock()) {
            if (object.justDiscovered() && !this_->cameraObject_)
                this_->cameraObject_ = object;
        }
    });

    battleFederate_->getObjectClass("Terrain").observe([weak_](ObjectRef object) {
        if (auto this_ = weak_.lock()) {
            this_->handleTerrainChanged_(object);
        }
    });

    battleFederate_->getObjectClass("Shape").observe([weak_](const ObjectRef& object) {
        auto this_ = weak_.lock();
        if (this_ && object.justDiscovered()) {
            this_->discoverShape_(object);
        }
    });

    battleFederate_->startup(battleFederationId);
}


Promise<void> BattleView::shutdown_() {
    co_await *Strand::getRender();

    viewModel_.units.clear();

    delete renderTerrain_;
    renderTerrain_ = nullptr;
    delete renderWater_;
    renderWater_ = nullptr;
    delete renderSky_;
    renderSky_ = nullptr;
    delete renderRange_;
    renderRange_ = nullptr;
    delete renderFacing_;
    renderFacing_ = nullptr;
    delete renderPhantom_;
    renderPhantom_ = nullptr;
    delete renderMarker_;
    renderMarker_ = nullptr;
    delete renderMovement_;
    renderMovement_ = nullptr;
    delete renderSelection_;
    renderSelection_ = nullptr;
    delete renderDebug_;
    renderDebug_ = nullptr;

    co_await battleFederate_->shutdown();
}


const TerrainMap& BattleView::getTerrainMap() const {
    return *viewModel_.terrainMap;
}


const HeightMap& BattleView::getHeightMap() const {
    return viewModel_.terrainMap->getHeightMap();
}


Viewport& BattleView::getViewport() {
    return *viewport_;
}


void BattleView::handleUnitDiscovered_(ObjectRef object) {
    auto* unitVM = new BattleVM::Unit{
            .object = object,
            .unitId = object.getObjectId()
    };
    viewModel_.units.emplace_back(unitVM);
    handleUnitChanged_(*unitVM);
}


void BattleView::handleUnitDestroyed_(ObjectId objectId) {
    viewModel_.units.erase(
            std::remove_if(viewModel_.units.begin(), viewModel_.units.end(), [objectId](const auto& x) {
                return x->unitId == objectId;
            }), viewModel_.units.end());
}


void BattleView::handleUnitChanged_(BattleVM::Unit& unitVM) {
    if (unitVM.object["alliance"].hasChanged()) {
        unitVM.allianceId = unitVM.object["alliance"_ObjectId];
    }
    if (unitVM.object["unitType"].hasChanged()) {
        if (auto unitType = unitVM.object["unitType"_value]; unitType.is_document()) {
            for (const auto& subunit : unitType["subunits"_value]) {
                auto weaponVM = BattleVM::Weapon{};
                for (const auto& weapon : subunit["weapons"_value]) {
                    for (const auto& missile : weapon["missiles"_value]) {
                        auto missileVM = BattleVM::MissileStats{};
                        missileVM.id = missile["id"_int];
                        missileVM.trajectoryShape = missile["trajectoryShape"_c_str] ?: "";
                        missileVM.releaseShape = missile["releaseShape"_c_str] ?: "";
                        missileVM.impactShape = missile["impactShape"_c_str] ?: "";
                        weaponVM.missileStats.push_back(missileVM);
                    }
                    unitVM.weapons.push_back(weaponVM);
                }
            }
        }
    }
    if (unitVM.object["marker"].hasChanged()) {
        unitVM.marker = toMarker_(unitVM.object["marker"_value]);
    }
}


BattleVM::Marker BattleView::toMarker_(const Value& value) {
    const char* textureName = value["texture"_c_str];
    auto texture = renderMarker_->getTexture(textureName);
    if (!texture.second) {
        battleFederate_->getServiceClass("_LoadTexture")
                .request(Struct{} << "name" << textureName << ValueEnd{})
                .then<void>([this_ = shared_from_this(), textureId = texture.first](const Value& response) {
                    auto data = response["data"_binary];
                    auto image = Image::decodePng(data.data, data.size);
                    if (!image) {
                        return LOG_E("LoadTexture: could not decode png image");
                    }
                    if (this_->renderMarker_) {
                        this_->renderMarker_->setTexture(textureId, *image);
                    }
                }).done();
    }

    const auto texgrid = value["texgrid"_float] ?: 1.0f;
    auto result = BattleVM::Marker{};
    result.texture = texture.first;
    for (const auto& layer : value["layers"_value]) {
        result.layers.push_back(toMarkerLayer_(layer, texgrid));
    }
    return result;
}


BattleVM::MarkerLayer BattleView::toMarkerLayer_(const Value& value, float texgrid) {
    auto result = BattleVM::MarkerLayer{};
    result.vertices[0].x = value["vertices"]["0"]["0"_float] / texgrid;
    result.vertices[0].y = value["vertices"]["0"]["1"_float] / texgrid;
    result.vertices[1].x = value["vertices"]["1"]["0"_float] / texgrid;
    result.vertices[1].y = value["vertices"]["1"]["1"_float] / texgrid;
    const auto state = value["state"_value];
    result.SetStateMatch(BattleVM::MarkerState::Allied, state["allied"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Command, state["command"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Dragged, state["dragged"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Friendly, state["friendly"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Hovered, state["hovered"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Hostile, state["hostile"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Routed, state["routed"_value]);
    result.SetStateMatch(BattleVM::MarkerState::Selected, state["selected"_value]);
    return result;
}


void BattleView::updateUnitElements_() {
    const auto& heightMap = getHeightMap();
    for (auto unitObject : battleFederate_->getObjectClass("Unit")) {
        auto i = std::find_if(viewModel_.units.begin(), viewModel_.units.end(), [unitId = unitObject.getObjectId()](const auto& x) {
            return x->unitId == unitId;
        });
        if (i == viewModel_.units.end()) {
            continue;
        }
        auto& unitPtr = *i;
        auto& unitVM = *unitPtr;

        unitVM.object = unitObject;

        const auto binary = unitObject["_fighters"_value]["..."_binary];
        const auto count = binary.size / sizeof(glm::vec3);
        if (unitVM.elements.size() > count) {
            unitVM.elements.erase(unitVM.elements.begin() + count, unitVM.elements.end());
        }
        bool friendly = unitVM.object["alliance"_ObjectId] == commanderAllianceId_;
        if (unitVM.elements.size() < count) {
            auto shape = viewModel_.GetShape(unitObject["unitType"_value]["subunits"]["0"]["element"]["shape"_c_str] ?: "");
            while (unitVM.elements.size() < count) {
                auto body = BattleVM::Body{shape};
                for (const auto& trajectory : body.shape->lines) {
                    body.state.lines.push_back({});
                }
                for (const auto& skin : body.shape->skins) {
                    body.state.skins.push_back({
                            .loop = BattleVM::Loop::findLoop(skin.loops, friendly ? BattleVM::LoopType::Friendly : BattleVM::LoopType::Hostile)
                    });
                }
                unitVM.elements.push_back({unitPtr, std::move(body)});
            }
        }
        auto* p = reinterpret_cast<const glm::vec3*>(binary.data);
        for (auto& element : unitVM.elements) {
            auto xy = p->xy();
            element.body.state.position = glm::vec3{ xy, heightMap.interpolateHeight(xy) };
            element.body.state.orientation = p->z;
            ++p;
        }
    }
}


void BattleView::handleTerrainChanged_(ObjectRef terrain) {
    if (terrain.justDiscovered()) {
        terrain_ = terrain;
    } else if (terrain.justDestroyed()) {
        terrain_ = {};
    }

    if (terrain_) {
        updateTerrain_();
    }
}

static std::unique_ptr<Image> imageFromUint8Matrix(const Value& matrix) {
    auto cols = matrix["cols"_int];
    auto rows = matrix["rows"_int];
    auto data = matrix["data"_binary];
    if (data.data && data.size == cols * rows) {
        auto ptr = std::shared_ptr<std::uint8_t>(new std::uint8_t[data.size], [](auto p) { delete[] p; });
        std::memcpy(ptr.get(), data.data, data.size);
        return std::make_unique<Image>(glm::ivec3{cols, rows, 1}, std::move(ptr));
    }
    return {};
}


void BattleView::updateTerrain_() {
    std::unique_ptr<Image> height{};
    std::unique_ptr<Image> woods{};
    std::unique_ptr<Image> water{};
    std::unique_ptr<Image> fords{};
    if (terrain_) {
        auto terrainHeight = terrain_["height"_value];
        height = imageFromUint8Matrix(terrainHeight["matrix"]);
        // auto factor = height["factor"_float];
        // auto offset = height["offset"_float];

        woods = imageFromUint8Matrix(terrain_["woods"_value]);
        water = imageFromUint8Matrix(terrain_["water"_value]);
        fords = imageFromUint8Matrix(terrain_["fords"_value]);
    }

    auto bounds = bounds2f{0, 0, 1024, 1024};
    auto terrainMap = new TerrainMap(bounds, std::move(height), std::move(woods), std::move(water), std::move(fords));

    auto& sharedMap = terrain_.acquireShared<TerrainMap*>();
    sharedMap = terrainMap;
    terrain_.releaseShared();

    if (acquireTerrainMap_()) {
        renderTerrain_->initialize();
        renderWater_->waterVertices_.setInvalid();
        setTreesDirty_(viewModel_.terrainMap->getBounds());
    }
    releaseTerrainMap_();
}


void BattleView::setTerrainDirty(TerrainFeature terrainFeature, bounds2f bounds) {
    renderTerrain_->setDirtyBounds(bounds);

    if (terrainFeature != TerrainFeature::Fords) {
        setTreesDirty_(bounds);
    }
    if (terrainFeature != TerrainFeature::Trees) {
        renderWater_->waterVertices_.setInvalid();
    }
}


void BattleView::setTreesDirty_(bounds2f bounds) {
    if (treesDirty_.empty()) {
        treesDirty_ = bounds;
    } else {
        treesDirty_.min = glm::min(treesDirty_.min, bounds.min);
        treesDirty_.max = glm::max(treesDirty_.max, bounds.max);
    }
}


bool BattleView::acquireTerrainMap_() {
    if (terrain_) {
        viewModel_.terrainMap = terrain_.acquireShared<TerrainMap*>();
        if (viewModel_.terrainMap)
            cameraState_->SetHeightMap(&viewModel_.terrainMap->getHeightMap(), false);
    }
    return viewModel_.terrainMap != nullptr;
}


void BattleView::releaseTerrainMap_() {
    if (terrain_)
        terrain_.releaseShared();
    viewModel_.terrainMap = nullptr;
    cameraState_->SetHeightMap(nullptr, false);
}


void BattleView::setLoopType_(BattleVM::LoopType& result, bool cond, BattleVM::LoopType flag) {
    if (cond) {
        static_assert(sizeof(unsigned long) >= sizeof(flag));
        result = static_cast<BattleVM::LoopType>(static_cast<unsigned long>(result) | static_cast<unsigned long>(flag));
    }
}

BattleVM::LoopType BattleView::toLoopType_(const Value& value) {
    auto result = BattleVM::LoopType::None;
    setLoopType_(result, value["dead"_bool], BattleVM::LoopType::Dead);
    setLoopType_(result, value["friendly"_bool], BattleVM::LoopType::Friendly);
    setLoopType_(result, value["hostile"_bool], BattleVM::LoopType::Hostile);
    return result;
}

std::vector<float> BattleView::toFloatVector_(const Value& value, float scale) {
    auto result = std::vector<float>{};
    for (const auto& item : value) {
        result.push_back(item._float() * scale);
    }
    return result;
}

BattleVM::Loop BattleView::toLoop_(const Value& value) {
    const auto* textureName = value["texture"_c_str];
    auto texture = renderBody_.getTexture(textureName);
    if (!texture.second) {
        battleFederate_->getServiceClass("_LoadTexture")
                .request(Struct{} << "name" << textureName << ValueEnd{})
                .then<void>([this_ = shared_from_this(), textureId = texture.first](const Value& response) {
                    auto data = response["data"_binary];
                    auto image = Image::decodePng(data.data, data.size);
                    if (!image) {
                        return LOG_E("LoadTexture: could not decode png image");
                    }
                    image->premultiplyAlpha();
                    this_->renderBody_.setTexture(textureId, *image);
                }).done();
    }

    const auto texgrid = value["texgrid"_float] ?: 1.0f;

    auto result = BattleVM::Loop{};
    result.type = toLoopType_(value["type"_value]);
    result.texture = texture.first;
    result.angles = toFloatVector_(value["angles"_value]);
    result.vertices = toFloatVector_(value["vertices"_value], 1.0f / texgrid);
    return result;
}

BattleVM::Skin BattleView::toSkin_(const Value& value) {
    auto result = BattleVM::Skin{};
    result.type = BattleVM::SkinType::Billboard;
    for (const auto& loop : value["loops"_value]) {
        result.loops.push_back(toLoop_(loop));
    }
    return result;
}

glm::vec4 BattleView::toColor_(const Value& value) {
    auto result = glm::vec4{};
    result.r = value["0"_float];
    result.g = value["1"_float];
    result.b = value["2"_float];
    result.a = value["3"_float];
    return result;
}

BattleVM::Line BattleView::toLine_(const Value& value) {
    auto result = BattleVM::Line{};
    result.deltas = toFloatVector_(value["deltas"]);
    for (const auto& color : value["colors"_value]) {
        result.colors.push_back(toColor_(color));
    }
    return result;
}

void BattleView::discoverShape_(const ObjectRef& object) {
    auto shape = std::make_unique<BattleVM::Shape>();
    shape->name = object["name"_c_str];
    shape->size.x = object["size"_value]["0"_float];
    shape->size.y = object["size"_value]["1"_float];
    shape->size.z = object["size"_value]["2"_float];
    for (const auto& skin : object["skins"_value]) {
        shape->skins.push_back(toSkin_(skin));
    }
    for (const auto& line : object["lines"_value]) {
        shape->lines.push_back(toLine_(line));
    }

    const auto updateTrees = shape->name == "tree";
    viewModel_.shapes[shape->name].emplace_back(shape.release());

    if (updateTrees) {
        if (acquireTerrainMap_()) {
            setTreesDirty_(viewModel_.terrainMap->getBounds());
        }
        releaseTerrainMap_();
    }
}


void BattleView::acquireBattleControllerFrame_() {
    for (auto& unitVM : viewModel_.units)
        unitVM->unitGestureMarker = ObjectRef{};
    for (auto object : battleFederate_->getObjectClass("_UnitGestureMarker")) {
        if (auto* unitVM = getUnitVM_(object["unit"_ObjectId]))
            unitVM->unitGestureMarker = object;
    }
}


void BattleView::addCasualty_(ObjectId unitId, glm::vec2 position) {
    if (auto* unitVM = getUnitVM_(unitId)) {
        const float weaponReach = unitVM->object["stats.maximumReach"_float];
        const bool friendly = unitVM->object["alliance"_ObjectId] == getAllianceId_();
        auto body = BattleVM::Body{
                .shape = viewModel_.GetShape(unitVM->object["unitType"_value]["subunits"]["0"]["element"]["shape"_c_str] ?: ""),
                .state = {
                        .position = glm::vec3(position, viewModel_.terrainMap->getHeightMap().interpolateHeight(position))
                }
        };
        for (const auto& trajectory : body.shape->lines) {
            body.state.lines.push_back({});
        }
        for (const auto& skin : body.shape->skins) {
            body.state.skins.push_back({
                    .loop = BattleVM::Loop::findLoop(skin.loops, BattleVM::LoopType::Dead)
            });
        }
        viewModel_.casualties.push_back({
                .body = std::move(body),
                .color = friendly ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{1.0f, 0.0f, 0.0f}
        });
    }
}


void BattleView::render_(Framebuffer* frameBuffer, BackgroundView* backgroundView) {
    if (!cameraObject_ || !cameraObject_["value"_value].has_value())
        return;

    cameraState_->SetViewportBounds(static_cast<bounds2f>(viewport_->getViewportBounds()), viewport_->getScaling());

    auto oldFramebuffer = viewport_->getFrameBuffer();
    viewport_->setFrameBuffer(frameBuffer);

    bool acquired = acquireTerrainMap_();
    if (acquired) {
        auto cameraValue = cameraObject_["value"_value];
        cameraState_->SetCameraPosition(cameraValue["position"_vec3]);
        cameraState_->SetCameraFacing(cameraValue["facing"_float]);
        cameraState_->SetCameraTilt(cameraValue["tilt"_float]);
        cameraState_->UpdateTransform();

        auto transform = cameraState_->GetTransform();

        updateUnitElements_();
        acquireBattleControllerFrame_();

        if (!treesDirty_.empty()) {
            battleAnimator_.UpdateVegetationBody(treesDirty_);
            treesDirty_ = bounds2f{};
        }

        battleAnimator_.UpdateElementTrajectory();

        renderSky_->skyVertices_.update(getCameraState().GetCameraDirection());
        renderSky_->update(renderSky_->skyVertices_);

        renderTerrain_->update(renderTerrain_->terrainVertices_);

        if (renderWater_->waterVertices_.invalid) {
            renderWater_->waterVertices_.update(getTerrainMap());
            renderWater_->update(renderWater_->waterVertices_);
        }

        renderBody_.bodyVertices_.update(*cameraState_, viewModel_);
        renderBody_.update(renderBody_.bodyVertices_);

        renderRange_->rangeVertices_.update(this);
        renderRange_->update(renderRange_->rangeVertices_);

        renderFacing_->facingVertices_.update(*this);
        renderFacing_->update(renderFacing_->facingVertices_);

        renderMovement_->movementVertices_.update(this);
        renderMovement_->update(renderMovement_->movementVertices_);

        renderPhantom_->phantomVertices_.update(this);
        renderPhantom_->update(renderPhantom_->phantomVertices_);

        renderMarker_->markerVertices_.update(*battleFederate_, this);
        renderMarker_->update(renderMarker_->markerVertices_);

        renderSelection_->selectionVertices_.update(getCameraState(), battleFederate_->getObjectClass("_UnitGestureGroup"));
        renderSelection_->update(renderSelection_->selectionVertices_);

        renderDebug_->debugVertices_.update(*battleFederate_);
        renderDebug_->update(renderDebug_->debugVertices_);

        renderTerrain_->preRenderSobel(*viewport_, transform);
        renderTerrain_->prepareHatchings();
        renderTerrain_->prerenderHatchings1();
        renderTerrain_->prerenderHatchings2(transform);
        renderTerrain_->renderGround(*viewport_, transform);
        renderTerrain_->renderSobelTexture(*viewport_);
        renderTerrain_->renderLines(*viewport_, transform);

        renderWater_->render(*viewport_, transform);
        renderBody_.render(*viewport_, *cameraState_);
        renderTerrain_->renderHatchings(*viewport_);
        renderRange_->render(*viewport_, transform);

        if (!renderMarker_->textureGroups_.empty()) {
            renderFacing_->render(*viewport_, *renderMarker_->textureGroups_.begin()->second.texture);
        }
        renderMovement_->render(*viewport_, transform);
        renderPhantom_->render(*viewport_, transform, cameraState_->GetCameraUpVector());
    }

    backgroundView->render(frameBuffer);

    if (acquired) {
        auto transform = cameraState_->GetTransform();

        renderTerrain_->renderShadow(*viewport_, transform);
        renderSky_->render(*viewport_);
        renderMarker_->render(*viewport_, *cameraState_);
        renderSelection_->render(*viewport_);
        renderDebug_->render(*viewport_, cameraState_->GetViewportBounds(), cameraState_->GetTransform());
    }

    viewport_->setFrameBuffer(oldFramebuffer);

    releaseTerrainMap_();
}


void BattleView::animate_(double secondsSinceLastUpdate) {
    if (acquireTerrainMap_()) {
        updateUnitElements_();
        acquireBattleControllerFrame_();

        updateSoundPlayer_();

        Strand::getMain()->setImmediate([soundDirector = soundDirector_, secondsSinceLastUpdate]() {
            soundDirector->Tick(secondsSinceLastUpdate);
        });

        const auto delta = static_cast<float>(secondsSinceLastUpdate);
        for (auto& casualty : viewModel_.casualties) {
            casualty.time += delta;
        }
        for (auto& unitVM : viewModel_.units) {
            unitVM->Animate(delta);
        }

        battleAnimator_.AnimateVolleys(delta);
        battleAnimator_.UpdateProjectileTrajectory();
        battleAnimator_.AnimateSmoke(delta);
    }
    releaseTerrainMap_();
}


/***/


BattleVM::Unit* BattleView::getUnitVM_(ObjectId unitId) const {
    if (unitId)
        for (auto& unitVM : viewModel_.units)
            if (unitVM->unitId == unitId)
                return unitVM.get();
    return nullptr;
}


void BattleView::updateSoundPlayer_() {
    int cavalryRunning = 0;
    int cavalryWalking = 0;
    int cavalryCount = 0;
    int infantryWalking = 0;
    int infantryRunning = 0;
    int friendlyUnits = 0;
    int enemyUnits = 0;

    for (const auto& unitVM : viewModel_.units) {
        if (const auto& unitObject = unitVM->object) {
            if (unitObject["stats.isCavalry"_bool]) {
                ++cavalryCount;
            }

            if (!unitObject["_routing"_bool]) {
                if (unitObject["alliance"_ObjectId] == getAllianceId_())
                    ++friendlyUnits;
                else
                    ++enemyUnits;
            }

            if (glm::length(unitObject["_destination"_vec2] - unitObject["_position"_vec2]) > 4.0f) {
                if (unitObject["stats.isCavalry"_bool]) {
                    if (unitObject["running"_bool])
                        ++cavalryRunning;
                    else
                        ++cavalryWalking;
                } else {
                    if (unitObject["running"_bool])
                        ++infantryRunning;
                    else
                        ++infantryWalking;
                }
            }
        }
    }

    int meleeCavalry = 0;
    int meleeInfantry = 0;
    //ObjectId winnerAllianceId{};

    if (battleStatistics_) {
        meleeCavalry = battleStatistics_["countCavalryInMelee"_int];
        meleeInfantry = battleStatistics_["countInfantryInMelee"_int];
        //winnerAllianceId = _battleStatistics["winnerAlliance"_ObjectId];
    }

    Strand::getMain()->setImmediate([soundDirector = soundDirector_,
            cavalryWalking, cavalryRunning, cavalryCount,
            infantryWalking, infantryRunning,
            meleeInfantry, meleeCavalry,
            friendlyUnits, enemyUnits,
            allianceId = getAllianceId_()]() {
        soundDirector->UpdateInfantryWalking(infantryWalking != 0);
        soundDirector->UpdateInfantryRunning(infantryRunning != 0);
        soundDirector->UpdateCavalryWalking(cavalryWalking != 0);
        soundDirector->UpdateCavalryRunning(cavalryRunning != 0);
        soundDirector->UpdateCavalryCount(cavalryCount);
        soundDirector->UpdateMeleeCavalry(meleeCavalry != 0);
        soundDirector->UpdateMeleeInfantry(meleeInfantry != 0);
        soundDirector->UpdateMeleeCharging();

        //auto& musicDirector = soundDirector->GetMusicDirector();
        //musicDirector.UpdateUnitsMoving(infantryWalking + infantryRunning + cavalryWalking + cavalryRunning);
        //musicDirector.UpdateUnitsRunning(infantryRunning + cavalryRunning);
        //musicDirector.UpdateMeleeCavalry(meleeCavalry);
        //musicDirector.UpdateMeleeInfantry(meleeInfantry);
        //musicDirector.UpdateFriendlyUnits(friendlyUnits);
        //musicDirector.UpdateEnemyUnits(enemyUnits);

        //if (winnerAllianceId)
        //    musicDirector.UpdateOutcome(winnerAllianceId == allianceId ? 1 : -1);
        //else
        //    musicDirector.UpdateOutcome(0);
    });
}


ObjectRef BattleView::getAlliance_(ObjectId allianceId) const {
    return battleFederate_->getObject(allianceId);
}


int BattleView::getAlliancePosition_(ObjectId allianceId) const {
    if (auto alliance = getAlliance_(allianceId))
        return alliance["position"_int];
    return 0;
}


bool BattleView::isCommandable_(const ObjectRef& unit) const {
    if (auto commander = battleFederate_->getObject(unit["commander"_ObjectId])) {
        if (const char* playerId = commander["playerId"_c_str]) {
            if (playerId_ == playerId)
                return true;
        }
    }
    return unit["delegated"_bool] && isPlayerAlliance_(unit["alliance"_ObjectId]);
}


bool BattleView::isPlayerAlliance_(ObjectId allianceId) const {
    for (const auto& commander : battleFederate_->getObjectClass("Commander")) {
        if (const char* playerId = commander["playerId"_c_str]) {
            if (playerId_ == playerId && commander["alliance"_ObjectId] == allianceId) {
                return true;
            }
        }
    }
    return false;
}


bool BattleView::shouldShowMovementPath_(const ObjectRef& unit) const {
    if (!isPlayerAlliance_(unit["alliance"_ObjectId]) || unit["_routing"_bool])
        return false;

    auto path = unit["_path"_value];
    int n = 0;
    glm::vec2 p;
    float d = 0;
    for (const auto& i : path) {
        auto q = i._vec2();
        if (n++ != 0)
            d += glm::distance(p, q);
        if (n > 1 || d > 8)
            return true;
        p = q;
    }
    return false;
}
