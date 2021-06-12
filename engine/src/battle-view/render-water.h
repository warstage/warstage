// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_WATER_H
#define WARSTAGE__BATTLE_VIEW__RENDER_WATER_H

#include "./shaders.h"
#include "image/image.h"
#include "graphics/vertex-buffer.h"

class Graphics;
class TerrainMap;
class Viewport;


struct WaterVertices {
    std::vector<Vertex<_2f>> inside{};
    std::vector<Vertex<_2f>> border{};
    bounds2f bounds{};
    bool invalid{};
    void setInvalid() { invalid = true; }
    void update(const TerrainMap& terrainMap);
};


class WaterRenderer {
    VertexBuffer_2f vertexBufferInside_;
    VertexBuffer_2f vertexBufferBorder_;
    std::unique_ptr<Pipeline> pipelineInside_{};
    std::unique_ptr<Pipeline> pipelineBorder_{};
    bounds2f bounds_{};

public:
    WaterVertices waterVertices_;

    explicit WaterRenderer(Graphics& graphics);
    void update(const WaterVertices& vertices);
    void render(const Viewport& viewport, const glm::mat4& transform);
};


#endif
