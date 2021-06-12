// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-background.h"
#include "geometry/geometry.h"
#include "graphics/graphics.h"
#include <glm/gtc/matrix_transform.hpp>


BackgroundView::BackgroundView(Viewport& viewport) :
        graphics_{viewport.getGraphics()},
        viewport_(&viewport),
        pipeline_(viewport.getGraphics()->getGraphicsApi())
{



    auto& graphicsApi = graphics_->getGraphicsApi();
    vertexbuffer_ = std::make_unique<VertexBuffer_3f_4f>(graphicsApi);

    colors_[0] = glm::vec4{119, 164, 199, 255} / 255.0f;
    colors_[1] = glm::vec4{123, 171, 123, 255} / 255.0f;
    colors_[2] = glm::vec4{160, 143, 130, 255} / 255.0f;

    for (int i = 0; i < 12; ++i) {
        hexes_.emplace_back();
        auto& hex = hexes_.back();
        randomize(hex);
    }
}


void BackgroundView::animate(double secondsSinceLastUpdate) {
    mutex_.lock();

    float dt = static_cast<float>(secondsSinceLastUpdate);

    for (auto& hex : hexes_) {
        hex.center += hex.delta * dt;
        hex.timer += dt;
        if (hex.timer >= 2 * hex.duration)
            randomize(hex);
    }

    mutex_.unlock();
}


void BackgroundView::render(Framebuffer* frameBuffer) {
    mutex_.lock();
    auto oldFramebuffer = viewport_->getFrameBuffer();
    viewport_->setFrameBuffer(frameBuffer);

    auto bounds = static_cast<bounds2f>(viewport_->getViewportBounds());
    glm::vec2 screen = bounds.size();
    float radius = 0.7f * glm::length(screen);

    const float z = 0.995f;
    const float y = bounds.y().mix(0.5f);

    addRectangle(bounds.set_min_y(y), colors_[1], colors_[0]);
    addRectangle(bounds.set_max_y(y), colors_[2], colors_[1]);

    glm::vec3 p1[6];
    glm::vec3 p2[6];

    for (auto& hex : hexes_) {
        glm::vec2 p{hex.center.x * screen.x, hex.center.y * screen.y};
        float r2 = hex.radius * radius;
        float r1 = r2 * 0.9f;
        for (int i = 0; i < 6; ++i) {
            auto d = vector2_from_angle(i * glm::two_pi<float>() / 6);
            p1[i] = {p + d * r1, z};
            p2[i] = {p + d * r2, z};
        }

        float alpha = hex.timer / hex.duration;
        if (alpha > 1)
            alpha = 2 - alpha;

        auto ca = glm::vec4{hex.color,  alpha * hex.alpha};
        auto c0 = glm::vec4{hex.color, 0};
        for (int i = 0; i < 6; ++i) {
            int j = i == 5 ? 0 : i + 1;
            vertices_.push_back({{p, z}, ca});
            vertices_.push_back({p1[i], ca});
            vertices_.push_back({p1[j], ca});
            vertices_.push_back({p1[i], ca});
            vertices_.push_back({p2[i], c0});
            vertices_.push_back({p1[j], ca});
            vertices_.push_back({p2[i], c0});
            vertices_.push_back({p2[j], c0});
            vertices_.push_back({p1[j], ca});
        }
    }

    vertexbuffer_->updateVBO(vertices_);
    vertices_.clear();

    glm::mat4 transform = getTransform();

    Pipeline{graphics_->getPipelineInitializer<GradientShader_3f>()}
            .setVertices(GL_TRIANGLES, *vertexbuffer_, {"position", "color"})
            .setUniform("transform", transform)
            .setUniform("point_size", 1.0f)
            .setDepthTest(true)
            .render(*viewport_);

    viewport_->setFrameBuffer(oldFramebuffer);
    mutex_.unlock();
}


glm::mat4 BackgroundView::getTransform() const {
    glm::mat4 result{1};
    bounds2i bounds = viewport_->getViewportBounds();

    result = glm::translate(result, glm::vec3{-1, -1, 0});
    result = glm::scale(result, glm::vec3{glm::vec2{2, 2} / (glm::vec2)bounds.size(), 1});

    return result;
}


void BackgroundView::addRectangle(const bounds2f& bounds, const glm::vec4& c1, const glm::vec4& c2) {
    const float z = 0.995f;

    vertices_.push_back({{bounds.min.x, bounds.min.y, z}, c1});
    vertices_.push_back({{bounds.min.x, bounds.max.y, z}, c2});
    vertices_.push_back({{bounds.max.x, bounds.max.y, z}, c2});
    vertices_.push_back({{bounds.max.x, bounds.max.y, z}, c2});
    vertices_.push_back({{bounds.max.x, bounds.min.y, z}, c1});
    vertices_.push_back({{bounds.min.x, bounds.min.y, z}, c1});
}


void BackgroundView::randomize(BackgroundView::Hex& hex) {
    float k = random(0.0f, 1.0f);

    hex.center.x = random(-0.1f, 1.1f);
    hex.center.y = random(-0.1f, 1.1f);
    hex.delta.x = random(-0.02f, 0.02f);
    hex.delta.y = random(-0.02f, 0.02f);
    hex.radius = random(0.05, 0.33);
    hex.color = {k, k, k};
    hex.alpha = random(0.05f, 0.15f);
    hex.timer = 0;
    hex.duration = random(4, 16);
}
