// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_DEBUG_H
#define WARSTAGE__BATTLE_VIEW__RENDER_DEBUG_H

#include "./shaders.h"
#include "graphics/vertex-buffer.h"

class Federate;
class Viewport;


struct DebugVertices {
    std::vector<Vertex<_2f>> vertices2 = {};
    std::vector<Vertex<_3f>> vertices3 = {};
    void update(Federate& federate);
};


class DebugRenderer {
    VertexBuffer_2f buffer2_;
    VertexBuffer_3f buffer3_;
    Pipeline pipeline2d_;
    Pipeline pipeline3d_;

public:
    DebugVertices debugVertices_ = {};

    explicit DebugRenderer(Graphics& graphics);
    void update(const DebugVertices& vertices);
    void render(const Viewport& viewport, bounds2i bounds, const glm::mat4& transform);
};


#endif
