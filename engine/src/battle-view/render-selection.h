// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_SELECTION_H
#define WARSTAGE__BATTLE_VIEW__RENDER_SELECTION_H

#include "./shaders.h"
#include "graphics/vertex-buffer.h"
#include "runtime/object.h"

class CameraState;
class ObjectClass;


struct SelectionVertices {
    std::vector<Vertex<_2f, _4f>> vertices{};
    bounds2f bounds{};
    void update(const CameraState& camera, ObjectClass& unitGestureGroups);
    void addUnitGestureGroup(const CameraState& camera, const ObjectRef& unitGestureGroup);
    void renderRectangle(const bounds2f& bounds);
};


class SelectionRenderer {
    VertexBuffer_2f_4f vertexBuffer_;
    std::unique_ptr<Pipeline> pipeline_{};
    bounds2f bounds_{};

public:
    SelectionVertices selectionVertices_;

    explicit SelectionRenderer(Graphics& graphics);
    void update(const SelectionVertices& vertices);
    void render(const Viewport& viewport);
};


#endif
