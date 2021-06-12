// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-range.h"
#include "./battle-view.h"
#include "geometry/geometry.h"
#include "battle-model/terrain-map.h"
#include "battle-simulator/battle-simulator.h"
#include "graphics/graphics.h"
#include <glm/gtc/constants.hpp>


void RangeVertices::update(BattleView* battleView_) {
    vertices.clear();
    for (const auto& unitVM : battleView_->getUnits_()) {
        if (const auto& unitObject = unitVM->object) {
            if (battleView_->isPlayerAlliance_(unitObject["alliance"_ObjectId])) {
                auto index = vertices.size();
                if (index) {
                    vertices.push_back(vertices.back());
                    vertices.emplace_back();
                }
                render(battleView_, unitObject);
                if (index && index + 2 < vertices.size()) {
                    vertices[index + 1] = vertices[index + 2];
                }
            }
        }
    }
}


void RangeVertices::render(BattleView* battleView_, const ObjectRef& unit) {
    if (auto currentMissileTarget = battleView_->getUnitVM_(unit["missileTarget"_ObjectId])) {
        if (const auto& missileTargetState = currentMissileTarget->object)
            renderMissileTarget(battleView_, missileTargetState["_position"_vec2], unit, glm::vec3{255, 64, 64});
    } else {
        if (unit["stats.isMissile"_bool] && !unit["_moving"_bool] && !unit["_routing"_bool]) {
            auto center = unit["_position"_vec2];

            BattleVM::MissileRange missileRange;
            missileRange.angleStart = unit["_angleStart"_float];
            missileRange.angleLength = unit["_angleLength"_float];
            missileRange.minimumRange = unit["stats.minimumRange"_float];
            missileRange.maximumRange = unit["stats.maximumRange"_float];
            missileRange.actualRanges = DecodeRangeValues(unit["_rangeValues"_value]);

            renderMissileRange(battleView_, center, missileRange);
        }
        if (auto futureMissileTarget = battleView_->getUnitVM_(unit["missileTarget"_ObjectId])) {
            if (const auto& missileTargetState = futureMissileTarget->object)
                renderMissileTarget(battleView_, missileTargetState["_position"_vec2], unit, glm::vec3{96, 64, 64});
        }
    }
}


void RangeVertices::renderMissileRange(BattleView* battleView_, glm::vec2 center, const BattleVM::MissileRange& unitRange) {
    const float thickness = 8;

    glm::vec4 c0 = glm::vec4(255, 64, 64, 0) / 255.0f;
    glm::vec4 c1 = glm::vec4(255, 64, 64, 24) / 255.0f;

    float angleMinOuter = unitRange.angleStart;
    float angleMinInner = angleMinOuter + 0.03f;
    float angleMaxOuter = unitRange.angleStart + unitRange.angleLength;
    float angleMaxInner = angleMaxOuter - 0.03f;

    float range = unitRange.actualRanges.front();
    glm::vec2 p2 = range * vector2_from_angle(angleMinOuter);
    glm::vec2 p4 = range * vector2_from_angle(angleMinInner);
    glm::vec2 p5 = (range - thickness) * vector2_from_angle(angleMinInner);
    glm::vec2 p1 = unitRange.minimumRange * vector2_from_angle(angleMinOuter);
    glm::vec2 p3 = p1 + (p4 - p2);


    for (int i = 0; i <= 8; ++i) {
        float t = i / 8.0f;
        vertices.emplace_back(getPosition(battleView_, center + glm::mix(p3, p5, t)), c0);
        vertices.emplace_back(getPosition(battleView_, center + glm::mix(p1, p2, t)), c1);
    }

    vertices.emplace_back(getPosition(battleView_, center + p4), c1);
    vertices.emplace_back(getPosition(battleView_, center + p4), c1);
    vertices.emplace_back(getPosition(battleView_, center + p5), c0);

    int n = (int)unitRange.actualRanges.size();
    float angleDelta = (angleMaxInner - angleMinInner) / (n - 1);
    for (int i = 0; i < n; ++i) {
        range = unitRange.actualRanges[i];
        float angle = angleMinInner + i * angleDelta;
        vertices.emplace_back(getPosition(battleView_, center + (range - thickness) * vector2_from_angle(angle)), c0);
        vertices.emplace_back(getPosition(battleView_, center + range * vector2_from_angle(angle)), c1);
    }

    range = unitRange.actualRanges.back();
    p2 = range * vector2_from_angle(angleMaxOuter);
    p4 = range * vector2_from_angle(angleMaxInner);
    p5 = (range - thickness) * vector2_from_angle(angleMaxInner);
    p1 = unitRange.minimumRange * vector2_from_angle(angleMaxOuter);
    p3 = p1 + (p4 - p2);

    vertices.emplace_back(getPosition(battleView_, center + p4), c1);
    for (int i = 0; i <= 8; ++i) {
        float t = i / 8.0f;
        vertices.emplace_back(getPosition(battleView_, center + glm::mix(p2, p1, t)), c1);
        vertices.emplace_back(getPosition(battleView_, center + glm::mix(p5, p3, t)), c0);
    }
}


void RangeVertices::renderMissileTarget(BattleView* battleView_, glm::vec2 target, const ObjectRef& unit, glm::vec3 color) {
    glm::vec4 c0 = glm::vec4(color, 0) / 255.0f;
    glm::vec4 c1 = glm::vec4(color, 24) / 255.0f;

    auto formation = FormationFromBson(unit["_formation"_value]);

    glm::vec2 left = BattleSM::BattleModel::GetFrontLeft(formation, unit["_position"_vec2]);
    glm::vec2 right = left + formation.towardRight * (float)formation.numberOfFiles;
    glm::vec2 p;

    const float thickness = 4;
    const float radius_outer = 16;
    const float radius_inner = radius_outer - thickness;
    float radius_left = glm::distance(left, target);
    float radius_right = glm::distance(right, target);

    float angle_left = angle(left - target);
    float angle_right = angle(right - target);
    if (angle_left < angle_right)
        angle_left += 2 * glm::pi<float>();

    glm::vec2 delta = thickness * vector2_from_angle(angle_left + glm::half_pi<float>());

    if (!vertices.empty()) {
        vertices.push_back(vertices.back());
        vertices.emplace_back(getPosition(battleView_, left + delta), c0);
    }
    vertices.emplace_back(getPosition(battleView_, left + delta), c0);

    vertices.emplace_back(getPosition(battleView_, left), c1);

    for (int i = 7; i >= 1; --i) {
        float r = i / 8.0f * radius_left;
        if (r > radius_outer) {
            p = target + r * vector2_from_angle(angle_left);
            vertices.emplace_back(getPosition(battleView_, p + delta), c0);
            vertices.emplace_back(getPosition(battleView_, p), c1);
        }
    }

    p = target + radius_outer * vector2_from_angle(angle_left);
    vertices.emplace_back(getPosition(battleView_, p + delta), c0);
    vertices.emplace_back(getPosition(battleView_, p), c1);

    p = target + radius_inner * vector2_from_angle(angle_left);
    vertices.emplace_back(getPosition(battleView_, p + delta), c0);
    vertices.emplace_back(getPosition(battleView_, p), c0);

    for (int i = 0; i <= 24; ++i) {
        float a = angle_left - i * (angle_left - angle_right) / 24;
        if (i == 0) {
            vertices.push_back(vertices.back());
            vertices.emplace_back(getPosition(battleView_, target + radius_outer * vector2_from_angle(a)), c1);
        }
        vertices.emplace_back(getPosition(battleView_, target + radius_outer * vector2_from_angle(a)), c1);
        vertices.emplace_back(getPosition(battleView_, target + radius_inner * vector2_from_angle(a)), c0);
    }

    delta = thickness * vector2_from_angle(angle_right - glm::half_pi<float>());
    p = target + radius_inner * vector2_from_angle(angle_right);
    vertices.emplace_back(getPosition(battleView_, p + delta), c0);
    vertices.emplace_back(getPosition(battleView_, p + delta), c0);

    p = target + radius_outer * vector2_from_angle(angle_right);
    vertices.emplace_back(getPosition(battleView_, p), c1);
    vertices.emplace_back(getPosition(battleView_, p + delta), c0);

    for (int i = 1; i <= 7; ++i) {
        float r = i / 8.0f * radius_right;
        if (r > radius_outer) {
            p = target + r * vector2_from_angle(angle_right);
            vertices.emplace_back(getPosition(battleView_, p), c1);
            vertices.emplace_back(getPosition(battleView_, p + delta), c0);
        }
    }

    vertices.emplace_back(getPosition(battleView_, right), c1);
    vertices.emplace_back(getPosition(battleView_, right + delta), c0);
}


glm::vec3 RangeVertices::getPosition(BattleView* battleView_, glm::vec2 p) const {
    glm::vec3 result = battleView_->getHeightMap().getPosition(p, 1);
    if (result.z < 0.5f)
        result.z = 0.5f;
    return result;
}


RangeRenderer::RangeRenderer(Graphics& graphics) :
    vertexBuffer_{graphics.getGraphicsApi()} {
    pipeline_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<GradientShader_3f>());
}


void RangeRenderer::update(const RangeVertices& vertices) {
    vertexBuffer_.updateVBO(vertices.vertices);
}


void RangeRenderer::render(const Viewport& viewport, const glm::mat4& transform) {
    pipeline_->setVertices(GL_TRIANGLE_STRIP, vertexBuffer_, {"position", "color"})
            .setUniform("transform", transform)
            .setUniform("point_size", 1.0f)
            .setDepthTest(true)
            .render(viewport);
}
