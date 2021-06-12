// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_BACKGROUND_H
#define WARSTAGE__BATTLE_VIEW__RENDER_BACKGROUND_H

#include "./shaders.h"
#include "graphics/texture.h"
#include "graphics/vertex.h"
#include "graphics/vertex-buffer.h"
#include "graphics/viewport.h"
#include "async/strand.h"
#include <mutex>
#include <random>


class BackgroundView {
    struct Hex {
        glm::vec2 center;
        glm::vec2 delta;
        glm::vec3 color;
        float radius;
        float alpha;
        float timer;
        float duration;
    };

    Graphics* graphics_{};
    Viewport* viewport_;
    std::mutex mutex_{};
    std::random_device rd_{};
    std::mt19937 e_{rd_()};
    std::uniform_real_distribution<float> d_{0.0f, 1.0f};

    glm::vec4 colors_[3];
    std::vector<Hex> hexes_{};
    std::vector<Vertex<_3f, _4f>> vertices_{};
    std::unique_ptr<VertexBuffer_3f_4f> vertexbuffer_{};
    Pipeline pipeline_;

public:
    explicit BackgroundView(Viewport& viewport);

    void animate(double secondsSinceLastUpdate);

    void render(Framebuffer* frameBuffer);
    [[nodiscard]] glm::mat4 getTransform() const;

    void addRectangle(const bounds2f& bounds, const glm::vec4& c1, const glm::vec4& c2);
    void randomize(Hex& hex);

    float random(float min, float max) {
        return min + d_(e_) * (max - min);
    }
};


#endif
