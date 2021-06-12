// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_PHANTOM_H
#define WARSTAGE__BATTLE_VIEW__RENDER_PHANTOM_H

#include "./shaders.h"
#include "battle-model/battle-vm.h"
#include "graphics/vertex-buffer.h"

class BattleView;


struct PhantomVertices {
    std::vector<Vertex<_3f, _4f, _1f>> vertices{};
    void update(BattleView* battleView_);
    void renderElements(BattleView* battleView_, const ObjectRef& unit, glm::vec2 destination, float bearing, int opacity);
};


class PhantomRenderer {
    VertexBuffer_3f_4f_1f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};

public:
    PhantomVertices phantomVertices_;

    explicit PhantomRenderer(Graphics& graphics);
    void update(const PhantomVertices& vertices);
    void render(const Viewport& viewport, const glm::mat4& transform, const glm::vec3& cameraUpVector);
};


#endif
