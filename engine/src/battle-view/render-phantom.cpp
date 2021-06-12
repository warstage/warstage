// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-phantom.h"
#include "./battle-view.h"
#include "graphics/graphics.h"


void PhantomVertices::update(BattleView* battleView_) {
    vertices.clear();
    for (const auto& unitVM : battleView_->getUnits_()) {
        if (const auto& unitObject = unitVM->object) {
            if (auto unitGestureMarker = unitVM->unitGestureMarker) {
                auto path = DecodeArrayVec2(unitGestureMarker["path"_value]);
                if (!path.empty() && !unitGestureMarker["meleeTarget"_ObjectId]
                        && !unitGestureMarker["missileTarget"_ObjectId]) {
                    auto center = path.back();
                    float facing = unitGestureMarker["facing"_float];
                    renderElements(battleView_, unitObject, center, facing, 48);
                }
            }
        }
    }

    for (const auto& unitVM : battleView_->getUnits_()) {
        if (const auto& unitObject = unitVM->object) {
            if (battleView_->shouldShowMovementPath_(unitObject)
                    && !unitObject["meleeTarget"_ObjectId]) {
                renderElements(battleView_, unitObject, unitObject["_destination"_vec2], unitObject["facing"_float], 32);
            }
        }
    }
}


void PhantomVertices::renderElements(BattleView* battleView_, const ObjectRef& unit, glm::vec2 destination, float bearing, int opacity) {
    glm::vec4 color = unit["alliance"_ObjectId] != battleView_->getAllianceId_() ? glm::vec4(255, 0, 0, opacity) / 255.0f
            : (!battleView_->getCommanderId_() || battleView_->getCommanderId_() != unit["commander"_ObjectId]) ? glm::vec4(0, 64, 0, opacity) / 255.0f
                    : glm::vec4(0, 0, 255, opacity) / 255.0f;

    auto formation = FormationFromBson(unit["_formation"_value]);

    formation.SetDirection(bearing);
    glm::vec2 frontLeft = BattleSM::BattleModel::GetFrontLeft(formation, destination);

    int count = unit["_fighterCount"_int];
    for (int index = 0; index < count; ++index) {
        int rank = index % formation.numberOfRanks;
        int file = index / formation.numberOfRanks;
        glm::vec2 offsetRight = formation.towardRight * static_cast<float>(file);
        glm::vec2 offsetBack = formation.towardBack * static_cast<float>(rank);
        glm::vec3 p = battleView_->getHeightMap().getPosition(frontLeft + offsetRight + offsetBack, 0.5);
        vertices.emplace_back(p, color, 3.0);
    }
}

PhantomRenderer::PhantomRenderer(Graphics& graphics) :
    vertexBuffer_{graphics.getGraphicsApi()} {
    pipeline_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<BillboardColorShader>());
}


void PhantomRenderer::update(const PhantomVertices& vertices) {
    vertexBuffer_.updateVBO(vertices.vertices);
}


void PhantomRenderer::render(const Viewport& viewport, const glm::mat4& transform, const glm::vec3& cameraUpVector) {
    pipeline_->setVertices(GL_POINTS, vertexBuffer_, {"position", "color", "height"})
            .setUniform("transform", transform)
            .setUniform("upvector", cameraUpVector)
            .setUniform("viewport_height", 0.25f * static_cast<float>(viewport.getViewportBounds().y().size()))
            .setDepthTest(true)
            .render(viewport);
}
