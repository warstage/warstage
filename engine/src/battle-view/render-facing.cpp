// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-facing.h"
#include "./battle-view.h"
#include "./camera-state.h"
#include "battle-gestures/unit-controller.h"
#include "graphics/graphics.h"
#include <glm/gtc/constants.hpp>


void FacingVertices::update(const BattleView& battleView) {
    vertices.clear();
    for (const auto& unitVM : battleView.getUnits_()) {
        if (const auto& unitObject = unitVM->object) {
            if (battleView.isCommandable_(unitObject)
                    && unitObject["_standing"_bool]
                    && !unitObject["_routing"_bool]
                    && !unitObject["meleeTarget"_ObjectId]) {
                appendUnitFacingMarker(battleView, *unitVM);
            }
        }
    }

    for (const auto& unitVM : battleView.getUnits_()) {
        if (const auto& unitObject = unitVM->object) {
            if (battleView.shouldShowMovementPath_(unitObject) && unitObject["_moving"_bool]) {
                appendMovementFacingMarker(battleView, *unitVM);
            }
        }
    }

    for (const auto& unitVM : battleView.getUnits_()) {
        if (auto unitGestureMarker = unitVM->unitGestureMarker) {
            if (const auto& unitObject = unitVM->object) {
                if (battleView.isCommandable_(unitObject))
                    appendTrackingFacingMarker(battleView, unitGestureMarker);
            }
        }
    }
}


void FacingVertices::appendUnitFacingMarker(const BattleView& battleView, const BattleVM::Unit& unitVM) {
    if (const auto& unitObject = unitVM.object) {
        int xindex = 0;
        /*if (!unit["isDeployed"_bool]) {
            xindex = 1;
        } else*/ if (unitObject["missileTarget"_ObjectId] == unitObject.getObjectId()) {
            xindex = 11;
        } else {
            if (unitObject["_loading"_bool]) {
                xindex = 2 + (int)glm::round(9.0f * unitObject["_loadingProgress"_float]);
                xindex = glm::min(10, xindex);
            }
        }
        int yindex = 0;
        if (xindex >= 6) {
            xindex -= 6;
            yindex += 1;
        }

        float tx1 = xindex * 0.125f;
        float tx2 = tx1 + 0.125f;

        float ty1 = 0.75f + yindex * 0.125f;
        float ty2 = ty1 + 0.125f;

        CameraState* cameraState = &battleView.getCameraState();

        float facing = unitObject["facing"_float];

        bounds2f bounds = battleView.getCameraState().GetUnitFacingMarkerBounds(unitObject["_position"_vec2], facing);
        glm::vec2 p = bounds.mid();
        float size = bounds.y().size();
        float direction = xindex >= 2 || yindex != 0 ? -glm::half_pi<float>() : (facing - cameraState->GetCameraFacing());

        glm::vec2 d1 = size * vector2_from_angle(direction - glm::half_pi<float>() / 2.0f);
        glm::vec2 d2 = glm::vec2(d1.y, -d1.x);
        glm::vec2 d3 = glm::vec2(d2.y, -d2.x);
        glm::vec2 d4 = glm::vec2(d3.y, -d3.x);

        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d2), glm::vec2(tx1, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d4), glm::vec2(tx2, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty2));
    }
}


void FacingVertices::appendMovementFacingMarker(const BattleView& battleView, const BattleVM::Unit& unitVM) {
    if (const auto& unitObject = unitVM.object) {
        CameraState* cameraState = &battleView.getCameraState();

        bounds2f b = battleView.getCameraState().GetUnitFacingMarkerBounds(unitObject["_destination"_vec2], unitObject["facing"_float]);
        glm::vec2 p = b.mid();
        float size = b.y().size();
        float direction = unitObject["facing"_float] - cameraState->GetCameraFacing();

        glm::vec2 d1 = size * vector2_from_angle(direction - glm::half_pi<float>() / 2.0f);
        glm::vec2 d2 = glm::vec2(d1.y, -d1.x);
        glm::vec2 d3 = glm::vec2(d2.y, -d2.x);
        glm::vec2 d4 = glm::vec2(d3.y, -d3.x);

        float tx1 = 0.125f;
        float tx2 = 0.125f + 0.125f;

        float ty1 = 0.75f;
        float ty2 = 0.75f + 0.125f;

        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d2), glm::vec2(tx1, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d4), glm::vec2(tx2, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty1));
    }
}


void FacingVertices::appendTrackingFacingMarker(const BattleView& battleView, const ObjectRef& unitGestureMarker) {
    auto unitId = unitGestureMarker["unit"_ObjectId];
    auto missileTargetId = unitGestureMarker["missileTarget"_ObjectId];
    auto path = DecodeArrayVec2(unitGestureMarker["path"_value]);
    if (missileTargetId != unitId && !path.empty()) {
        CameraState* cameraState = &battleView.getCameraState();

        float facing = unitGestureMarker["facing"_float];

        bounds2f b = battleView.getCameraState().GetUnitFacingMarkerBounds(path.back(), facing);
        glm::vec2 p = b.mid();
        float size = b.y().size();
        float direction = facing - cameraState->GetCameraFacing();

        glm::vec2 d1 = size * vector2_from_angle(direction - glm::half_pi<float>() / 2.0f);
        glm::vec2 d2 = glm::vec2(d1.y, -d1.x);
        glm::vec2 d3 = glm::vec2(d2.y, -d2.x);
        glm::vec2 d4 = glm::vec2(d3.y, -d3.x);

        float tx1 = 0.125f;
        float tx2 = 0.125f + 0.125f;

        float ty1 = 0.75f;
        float ty2 = 0.75f + 0.125f;

        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d2), glm::vec2(tx1, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d3), glm::vec2(tx2, ty2));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d4), glm::vec2(tx2, ty1));
        vertices.emplace_back(cameraState->WindowToNormalized(p + d1), glm::vec2(tx1, ty1));
    }
}


FacingRenderer::FacingRenderer(Graphics& graphics) :
        vertexBuffer_{graphics.getGraphicsApi()} {
    pipeline_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<TextureShader_2f>());
}

void FacingRenderer::update(const FacingVertices& vertices) {
    vertexBuffer_.updateVBO(vertices.vertices);
}


void FacingRenderer::render(const Viewport& viewport, Texture& texture) {
    pipeline_->setVertices(GL_TRIANGLES, vertexBuffer_, {"position", "texcoord"})
        .setUniform("transform", glm::mat4{1})
        .setTexture("texture", &texture)
        .render(viewport);
}
