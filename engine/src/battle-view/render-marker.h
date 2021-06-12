// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_MARKER_H
#define WARSTAGE__BATTLE_VIEW__RENDER_MARKER_H

#include "./shaders.h"
#include "battle-simulator/battle-objects.h"
#include "battle-model/battle-vm.h"
#include "graphics/vertex-buffer.h"
#include "image/image.h"
#include "runtime/runtime.h"

class BattleView;
class CameraState;
class Viewport;


struct MarkerVertices {
    std::unordered_map<int, std::vector<Vertex<_3f, _1f, _2f, _2f, _4f>>> vertices = {};

    void update(Federate& battleFederate, BattleView* battleView_);

    BattleVM::MarkerState getMarkerState(BattleView* battleView_, ObjectId allianceId, ObjectId commanderId) const;
    BattleVM::MarkerState getMarkerState(BattleView* battleView_, const BattleVM::Unit& unitVM) const;

    void renderUnitMarker(BattleView* battleView_, const BattleVM::Unit& unitVM);
    void renderDraggingMarker(BattleView* battleView_, const BattleVM::Unit& unitVM);
    void renderDestinationMarker(BattleView* battleView_, const BattleVM::Unit& unitVM);
    void renderDeploymentMarker(BattleView* battleView_, const ObjectRef& deploymentUnit);
    void addVerticesMarker(const glm::vec3& pos, const BattleVM::Marker& marker, BattleVM::MarkerState state, float alpha);
};


class MarkerRenderer {
    struct TextureGroup {
        std::string name;
        VertexBuffer_3f_1f_2f_2f_4f buffer;
        std::unique_ptr<Texture> texture = nullptr;
    };

    Graphics* graphics_ = nullptr;
    int lastTextureId_ = 0;
    std::unique_ptr<Pipeline> pipelineMarkers_ = nullptr;

public:
    std::unordered_map<int, TextureGroup> textureGroups_ = {};

public:
    MarkerVertices markerVertices_;

    explicit MarkerRenderer(Graphics& graphics);

    std::pair<int, bool> getTexture(const std::string& textureName);
    void setTexture(int textureId, const Image& image);

    void update(const MarkerVertices& vertices);
    void render(const Viewport& viewport, const CameraState& cameraState);
};


#endif
