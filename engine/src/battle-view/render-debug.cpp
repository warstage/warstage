// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-debug.h"
#include "battle-gestures/camera-gesture.h"
#include "./camera-state.h"
#include "runtime/federate.h"
#include "graphics/graphics.h"
#include <glm/gtc/matrix_transform.hpp>


void DebugVertices::update(Federate& federate) {
    vertices2.clear();
    vertices3.clear();

    for (const auto& object : federate.getObjectClass("DebugScreen"))
        for (const auto& vertex : object["vertices"_value])
            vertices2.emplace_back(vertex.cast<glm::vec2>());

    for (const auto& object : federate.getObjectClass("DebugWorld"))
        for (const auto& vertex : object["vertices"_value])
            vertices3.emplace_back(vertex.cast<glm::vec3>());
}


DebugRenderer::DebugRenderer(Graphics& graphics) :
        pipeline2d_(graphics.getPipelineInitializer<PlainShader_2f>()),
        pipeline3d_(graphics.getPipelineInitializer<PlainShader_3f>()),
        buffer3_{graphics.getGraphicsApi()},
        buffer2_{graphics.getGraphicsApi()} {
}


void DebugRenderer::update(const DebugVertices& vertices) {
    buffer2_.updateVBO(vertices.vertices2);
    buffer3_.updateVBO(vertices.vertices3);
}


void DebugRenderer::render(const Viewport& viewport, bounds2i bounds, const glm::mat4& transform) {
    pipeline3d_
            .setVertices(GL_LINES, buffer3_, {"position"})
            .setUniform("transform", transform)
            .setUniform("point_size", 1.0f)
            .setUniform("color", glm::vec4(0, 0, 0, 0.8f))
            .setLineWidth(1.0f)
            .setDepthTest(true)
            .setDepthMask(false)
            .render(viewport);
    pipeline3d_
            .setUniform("color", glm::vec4(1, 1, 1, 0.2f))
            .render(viewport);

    auto transform2 = glm::mat4{1};
    transform2 = glm::translate(transform2, glm::vec3{-1.0f, -1.0f, 0.0f});
    transform2 = glm::scale(transform2, glm::vec3(2.0f / bounds.x().size(), 2.0f / bounds.y().size(), 1.0f));

    pipeline2d_
            .setVertices(GL_LINES, buffer2_, {"position"})
            .setUniform("transform", transform2)
            .setUniform("point_size", 1.0f)
            .setUniform("color", glm::vec4(0, 0, 0, 0.8f))
            .setLineWidth(1.0f)
            .setDepthTest(false)
            .setDepthMask(false)
            .render(viewport);
    pipeline2d_
            .setUniform("color", glm::vec4(1, 1, 1, 0.2f))
            .render(viewport);
}
