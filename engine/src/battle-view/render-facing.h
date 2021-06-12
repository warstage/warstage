// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_FACING_
#define WARSTAGE__BATTLE_VIEW__RENDER_FACING_

#include "./shaders.h"
#include "graphics/vertex.h"
#include "graphics/vertex-buffer.h"
#include "battle-model/battle-vm.h"

class BattleView;


struct FacingVertices {
    std::vector<Vertex<_2f, _2f>> vertices{};
    void update(const BattleView& battleView);
    void appendUnitFacingMarker(const BattleView& battleView, const BattleVM::Unit& unitVM);
    void appendMovementFacingMarker(const BattleView& battleView, const BattleVM::Unit& unitVM);
    void appendTrackingFacingMarker(const BattleView& battleView, const ObjectRef& unitGestureMarker);
};


class FacingRenderer {
    VertexBuffer_2f_2f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};

public:
    FacingVertices facingVertices_ = {};

    explicit FacingRenderer(Graphics& graphics);
    void update(const FacingVertices& vertices);
    void render(const Viewport& viewport, Texture& texture);

};


#endif
