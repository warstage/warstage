// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_RANGE_H
#define WARSTAGE__BATTLE_VIEW__RENDER_RANGE_H

#include "./shaders.h"
#include "battle-model/battle-vm.h"
#include "battle-simulator/battle-objects.h"
#include "graphics/vertex.h"
#include "graphics/vertex-buffer.h"

class BattleView;


struct RangeVertices {
    std::vector<Vertex<_3f, _4f>> vertices{};
    void update(BattleView* battleView_);
    void render(BattleView* battleView_, const ObjectRef& unit);
    void renderMissileRange(BattleView* battleView_, glm::vec2 center, const BattleVM::MissileRange& unitRange);
    void renderMissileTarget(BattleView* battleView_, glm::vec2 target, const ObjectRef& unit, glm::vec3 color);
    [[nodiscard]] glm::vec3 getPosition(BattleView* battleView_, glm::vec2 p) const;
};


class RangeRenderer {
    VertexBuffer_3f_4f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};

public:
    RangeVertices rangeVertices_;

    explicit RangeRenderer(Graphics& graphics);
    void update(const RangeVertices& vertices);
    void render(const Viewport& viewport, const glm::mat4& transform);
};


#endif
