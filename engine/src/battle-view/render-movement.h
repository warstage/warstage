// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_MOVEMENT_H
#define WARSTAGE__BATTLE_VIEW__RENDER_MOVEMENT_H

#include "./shaders.h"
#include "battle-model/battle-vm.h"
#include "graphics/vertex-buffer.h"

class BattleView;
class HeightMap;


struct MovementVertices {
    std::vector<Vertex<_3f, _4f>> vertices{};
    void update(BattleView* battleView_);
    void renderMovementPath(BattleView* battleView_, const BattleVM::Unit& unitVM);
    void renderTrackingPath(BattleView* battleView_, const ObjectRef& unitGestureMarker);
    void renderOrientation(BattleView* battleView_, const ObjectRef& unitGestureMarker);
};


class RenderPath {
    const HeightMap& heightMap_;
    glm::vec4 color_;
    float offset_;

public:
    explicit RenderPath(const HeightMap& heightMap);
    ~RenderPath();

    void path(std::vector<Vertex<_3f, _4f>>& vertices, std::vector<glm::vec2> path, int mode);

    void renderPath_(std::vector<Vertex<_3f, _4f>>& vertices, const std::vector<glm::vec2>& path);
};

class MovementRenderer {
    VertexBuffer_3f_4f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};

public:
    MovementVertices movementVertices_ = {};

    explicit MovementRenderer(Graphics& graphics);
    void update(const MovementVertices& vertices);
    void render(const Viewport& viewport, const glm::mat4& transform);
};


#endif
