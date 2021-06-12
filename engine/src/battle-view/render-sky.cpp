// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-sky.h"
#include "./camera-state.h"
#include "graphics/graphics.h"
#include <glm/gtc/matrix_transform.hpp>


void SkyVertices::update(glm::vec3 cameraDirection) {
    float y = cameraDirection.z;
    float x = sqrtf(1 - y * y);
    float a = 1 - fabsf(atan2f(y, x) / (float)M_PI_2);
    float blend = bounds1f(0, 1).clamp(3.0f * (a - 0.3f));
    float d = 0.9925f;

    glm::vec4 c1 = glm::vec4{56, 165, 230, 0} / 255.0f;
    glm::vec4 c2 = glm::vec4{160, 207, 243, 255} / 255.0f;
    c2.a = blend;

    vertices.clear();
    vertices.emplace_back(glm::vec3{-1, -0.6f, d}, c1);
    vertices.emplace_back(glm::vec3{-1, 1, d}, c2);
    vertices.emplace_back(glm::vec3{1, 1, d}, c2);
    vertices.emplace_back(glm::vec3{1, 1, d}, c2);
    vertices.emplace_back(glm::vec3{1, -0.6f, d}, c1);
    vertices.emplace_back(glm::vec3{-1, -0.6f, d}, c1);
}


SkyRenderer::SkyRenderer(Graphics& graphics) :
        vertexBuffer_{graphics.getGraphicsApi()} {
    pipeline_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<GradientShader_3f>());
}

void SkyRenderer::update(const SkyVertices& vertices) {
    vertexBuffer_.updateVBO(vertices.vertices);
}


void SkyRenderer::render(const Viewport& viewport) {
    pipeline_->setVertices(GL_TRIANGLES, vertexBuffer_, {"position", "color"})
            .setUniform("transform", glm::mat4{1})
            .setUniform("point_size", 1.0f)
            .setDepthTest(true)
            .render(viewport);
}
