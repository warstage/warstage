// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-selection.h"
#include "./camera-state.h"
#include "graphics/program.h"
#include "graphics/graphics.h"
#include "runtime/object-class.h"
#include <glm/gtc/matrix_transform.hpp>

void SelectionVertices::update(const CameraState& camera, ObjectClass& unitGestureGroups) {
    vertices.clear();
    for (const auto& unitGestureGroup : unitGestureGroups) {
        addUnitGestureGroup(camera, unitGestureGroup);
    }
    bounds = (bounds2f)camera.GetViewportBounds();
}


void SelectionVertices::addUnitGestureGroup(const CameraState& camera, const ObjectRef& unitGestureGroup) {
    if (unitGestureGroup["selectionAnchor"_value].has_value() && unitGestureGroup["selectionPoint"_value].has_value()) {
        auto anchor = unitGestureGroup["selectionAnchor"_vec3];
        auto point = unitGestureGroup["selectionPoint"_vec3];

        auto p1 = camera.ContentToWindow(anchor);
        auto p2 = camera.ContentToWindow(point);
        bounds2f bounds{std::min(p1.x, p2.x), std::min(p1.y, p2.y), std::max(p1.x, p2.x), std::max(p1.y, p2.y)};
        if (!bounds.empty()) {
            renderRectangle(bounds);
        }

        float s = camera.GetUnitIconSizeLimit().mid();
        float r = std::max(2.0f, s / 8.0f);
        float d = std::max(bounds.x().radius(), bounds.y().radius()) / 4.0f;
        auto outer = bounds2f{bounds.mid()}.add_radius(s - d);

        if (outer.min.x < bounds.min.x) {
            renderRectangle({outer.min.x, bounds.min.y, bounds.min.x, bounds.min.y + r});
            renderRectangle({outer.min.x, bounds.max.y - r, bounds.min.x, bounds.max.y});
        }
        if (outer.min.y < bounds.min.y) {
            renderRectangle({bounds.min.x, bounds.min.y, bounds.min.x + r, outer.min.y});
            renderRectangle({bounds.max.x - r, bounds.min.y, bounds.max.x, outer.min.y});
        }
        if (outer.max.x > bounds.max.x) {
            renderRectangle({bounds.max.x, bounds.max.y - r, outer.max.x, bounds.max.y});
            renderRectangle({bounds.max.x, bounds.min.y, outer.max.x, bounds.min.y + r});
        }
        if (outer.max.y > bounds.max.y) {
            renderRectangle({bounds.max.x - r, bounds.max.y, bounds.max.x, outer.max.y});
            renderRectangle({bounds.min.x, bounds.max.y, bounds.min.x + r, outer.max.y});
        }
    }
}


void SelectionVertices::renderRectangle(const bounds2f& bounds) {
    glm::vec4 c{1.0f, 1.0f, 1.0f, 0.3f};
    vertices.emplace_back(glm::vec2{bounds.min.x, bounds.min.y}, c);
    vertices.emplace_back(glm::vec2{bounds.min.x, bounds.max.y}, c);
    vertices.emplace_back(glm::vec2{bounds.max.x, bounds.max.y}, c);
    vertices.emplace_back(glm::vec2{bounds.max.x, bounds.max.y}, c);
    vertices.emplace_back(glm::vec2{bounds.max.x, bounds.min.y}, c);
    vertices.emplace_back(glm::vec2{bounds.min.x, bounds.min.y}, c);
}



SelectionRenderer::SelectionRenderer(Graphics& graphics) :
        vertexBuffer_{graphics.getGraphicsApi()} {
    pipeline_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<GradientShader_2f>());
}


void SelectionRenderer::update(const SelectionVertices& vertices) {
    vertexBuffer_.updateVBO(vertices.vertices);
    bounds_ = vertices.bounds;
}


void SelectionRenderer::render(const Viewport& viewport) {
    auto transform = glm::mat4{1};
    transform = glm::translate(transform, glm::vec3{-1.0f, -1.0f, 0.0f});
    transform = glm::scale(transform, glm::vec3(2.0f / bounds_.x().size(), 2.0f / bounds_.y().size(), 1.0f));

    pipeline_->setVertices(GL_TRIANGLES, vertexBuffer_, {"position", "color"})
            .setUniform("transform", transform)
            .render(viewport);
}
