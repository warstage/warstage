// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-marker.h"
#include "./battle-view.h"
#include "battle-gestures/unit-controller.h"
#include "graphics/texture.h"
#include "graphics/graphics.h"


void MarkerVertices::update(Federate& battleFederate, BattleView* battleView_) {
    for (auto& i : vertices) {
        i.second.clear();
    }
    for (const auto& object : battleFederate.getObjectClass("DeploymentUnit")) {
        renderDeploymentMarker(battleView_, object);
    }

    for (const auto& unitVM : battleView_->getUnits_()) {
        renderDestinationMarker(battleView_, *unitVM);
    }

    for (const auto& unitVM : battleView_->getUnits_()) {
        renderUnitMarker(battleView_, *unitVM);
    }

    for (const auto& unitVM : battleView_->getUnits_()) {
        renderDraggingMarker(battleView_, *unitVM);
    }
}



BattleVM::MarkerState MarkerVertices::getMarkerState(BattleView* battleView_, ObjectId allianceId, ObjectId commanderId) const {
    if (allianceId != battleView_->getAllianceId_()) {
        return BattleVM::MarkerState::Hostile;
    }
    if (!battleView_->getCommanderId_() || battleView_->getCommanderId_() != commanderId) {
        return BattleVM::MarkerState::Allied;
    }
    return BattleVM::MarkerState::Friendly;
}


BattleVM::MarkerState MarkerVertices::getMarkerState(BattleView* battleView_, const BattleVM::Unit& unitVM) const {
    auto result = BattleVM::MarkerState::None;

    const float routingBlinkTime = unitVM.GetRoutingBlinkTime();
    const bool routingIndicator = unitVM.object["_routing"_bool]
            || (routingBlinkTime != 0 && bounds1f{0.0f, 0.2f}.contains(unitVM.routingTimer));

    if (routingIndicator) {
        result = result | BattleVM::MarkerState::Routed;
    } else {
        result = result | getMarkerState(battleView_, unitVM.object["alliance"_ObjectId], unitVM.object["commander"_ObjectId]);
    }

    if (auto unitGestureMarker = unitVM.unitGestureMarker) {
        if (!unitGestureMarker["selectionMode"_bool]) {
            result = result | BattleVM::MarkerState::Selected;
            if (unitGestureMarker["isPreliminary"_bool]) {
                result = result | BattleVM::MarkerState::Hovered;
            }
        }
    }

    if (battleView_->isCommandable_(unitVM.object)) {
        result = result | BattleVM::MarkerState::Command;
    }

    return result;
}


void MarkerVertices::renderUnitMarker(BattleView* battleView_, const BattleVM::Unit& unitVM) {
    if (!unitVM.object) {
        return;
    }
    const auto position = battleView_->getHeightMap().getPosition(unitVM.object["_position"_vec2], 0);
    const auto state = getMarkerState(battleView_, unitVM);
    addVerticesMarker(position, unitVM.marker, state, 1.0f);
}


void MarkerVertices::renderDraggingMarker(BattleView* battleView_, const BattleVM::Unit& unitVM) {
    if (!unitVM.unitGestureMarker || unitVM.unitGestureMarker["meleeTarget"_ObjectId]) {
        return;
    }
    const auto path = DecodeArrayVec2(unitVM.unitGestureMarker["path"_value]);
    if (path.empty()) {
        return;
    }
    const auto position = battleView_->getHeightMap().getPosition(path.back(), 0);
    const auto state = getMarkerState(battleView_, unitVM) | BattleVM::MarkerState::Dragged;
    addVerticesMarker(position, unitVM.marker, state, 1.0f);
}


void MarkerVertices::renderDestinationMarker(BattleView* battleView_, const BattleVM::Unit& unitVM) {
    if (!unitVM.object || unitVM.object["meleeTarget"_ObjectId] || !battleView_->shouldShowMovementPath_(unitVM.object)) {
        return;
    }
    const auto destination = unitVM.object["_destination"_vec2];
    const auto path = DecodeArrayVec2(unitVM.object["_path"_value]);
    if (path.size() <= 2 && glm::length(unitVM.object["_position"_vec2] - destination) < 25.0f) {
        return;
    }
    const auto position = battleView_->getHeightMap().getPosition(destination, 0.5f);
    const auto state = getMarkerState(battleView_, unitVM) | BattleVM::MarkerState::Dragged;
    addVerticesMarker(position, unitVM.marker, state, 1.0f);
}


void MarkerVertices::renderDeploymentMarker(BattleView* battleView_, const ObjectRef& deploymentUnit) {
    const auto allianceId = deploymentUnit["alliance"_ObjectId];
    const auto commanderId = deploymentUnit["commander"_ObjectId] ?: battleView_->getCommanderId_();
    if (!(deploymentUnit["hostingPlayerId"_bool] || battleView_->isPlayerAlliance_(allianceId))) {
        return;
    }

    const auto& markerValue = deploymentUnit["marker"_value];
    if (!markerValue.is_defined()) {
        return;
    }
    const auto marker = battleView_->toMarker_(markerValue);

    const bool reinforcement = deploymentUnit["reinforcement"_bool];
    const bool dragging = deploymentUnit["dragging"_bool];

    if (reinforcement || !dragging) {
        auto position = deploymentUnit["position"_vec2];
        auto p = battleView_->getHeightMap().getPosition(position, 0);
        auto state = getMarkerState(battleView_, allianceId, commanderId);
        addVerticesMarker(p, marker, state, 1.0f);
    }

    if (dragging) {
        auto position = deploymentUnit["_position"_vec2];
        auto p = battleView_->getHeightMap().getPosition(position, 0);
        auto state = getMarkerState(battleView_, allianceId, commanderId);
        auto alpha = deploymentUnit["_deleting"_bool] ? 0.5f : 1.0f;
        addVerticesMarker(p, marker, state, alpha);
    }
}

void MarkerVertices::addVerticesMarker(const glm::vec3& pos, const BattleVM::Marker& marker, BattleVM::MarkerState state, const float alpha) {
    const auto color = glm::vec4{1.0f, 1.0f, 1.0f, alpha};
    for (const auto& layer : marker.layers) {
        if (layer.IsMatch(state)) {
            vertices[marker.texture].emplace_back(pos, 32, layer.vertices[0], layer.vertices[1] - layer.vertices[0], color);
        }
    }
}



MarkerRenderer::MarkerRenderer(Graphics& graphics) :
        graphics_{&graphics} {
    pipelineMarkers_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<BillboardMarkerColorShader>());
}


std::pair<int, bool> MarkerRenderer::getTexture(const std::string& textureName) {
    for (const auto& textureGroup : textureGroups_) {
        if (textureGroup.second.name == textureName) {
            return {textureGroup.first, true};
        }
    }
    int textureId = ++lastTextureId_;
    textureGroups_.emplace(textureId, TextureGroup{
            .name = textureName,
            .buffer = VertexBuffer_3f_1f_2f_2f_4f(graphics_->getGraphicsApi()),
    });
    return {textureId, false};
}


void MarkerRenderer::setTexture(int textureId, const Image& image) {
    auto i = textureGroups_.find(textureId);
    assert(i != textureGroups_.end());
    auto& texture = i->second.texture;
    texture = std::make_unique<Texture>(graphics_->getGraphicsApi());
    texture->load(image);
}


void MarkerRenderer::update(const MarkerVertices& vertices) {
    for (auto& textureGroup : textureGroups_) {
        auto i = vertices.vertices.find(textureGroup.first);
        if (i != vertices.vertices.end()) {
            textureGroup.second.buffer.updateVBO(i->second);
        } else {
            textureGroup.second.buffer.updateVBO({});
        }
    }
}


void MarkerRenderer::render(const Viewport& viewport, const CameraState& cameraState) {
    auto transform = cameraState.GetTransform();
    auto upvector = cameraState.GetCameraUpVector();
    bounds1f sizeLimit = cameraState.GetUnitIconSizeLimit();
    float viewportHeight = viewport.getViewportBounds().y().size();

    for (auto& textureGroup : textureGroups_) {
        if (textureGroup.second.texture) {
            pipelineMarkers_->setVertices(GL_POINTS, textureGroup.second.buffer, {"position", "height", "texcoord", "texsize", "color"})
                    .setUniform("transform", transform)
                    .setTexture("texture", textureGroup.second.texture.get())
                    .setUniform("upvector", upvector)
                    .setUniform("viewport_height", viewportHeight)
                    .setUniform("min_point_size", sizeLimit.min)
                    .setUniform("max_point_size", sizeLimit.max)
                    .setDepthTest(false)
                    .render(viewport);
        }
    }
}
