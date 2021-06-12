// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_SKY_H
#define WARSTAGE__BATTLE_VIEW__RENDER_SKY_H

#include "./shaders.h"
#include "graphics/program.h"


struct SkyVertices {
    std::vector<Vertex<_3f, _4f>> vertices;
    void update(glm::vec3 cameraDirection);
};


class SkyRenderer {
    VertexBuffer_3f_4f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};

public:
    SkyVertices skyVertices_;

    explicit SkyRenderer(Graphics& graphics);
    void update(const SkyVertices& vertices);
    void render(const Viewport& viewport);
};


#endif
