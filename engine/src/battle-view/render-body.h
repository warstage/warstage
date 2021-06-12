// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_BODY_H
#define WARSTAGE__BATTLE_VIEW__RENDER_BODY_H

#include "image/image.h"
#include "graphics/texture.h"
#include "graphics/vertex-buffer.h"
#include "battle-model/battle-vm.h"
#include "./shaders.h"
#include <vector>

class CameraState;
class HeightMap;
class Viewport;


struct BodyVertices {
    std::vector<Vertex<_3f, _4f, _1f>> casualties = {};
    std::vector<Vertex<_3f>> weapons = {};
    std::vector<Vertex<_3f, _4f>> volleys = {};
    std::unordered_map<int, std::vector<Vertex<_3f, _1f, _2f, _2f>>> billboards = {};

    void update(const CameraState& cameraState, const BattleVM::Model& model);

    void addCasualties(const BattleVM::Model& model);
    void addWeapons(const BattleVM::Model& model);
    void appendWeapons(const BattleVM::Unit& unitVM);
    void addVolleys(const BattleVM::Model& model);
    void appendVolley(const BattleVM::Volley& volley);
    void addBillboards(const CameraState& cameraState, const BattleVM::Model& model);
    void addBillboardVertices(const CameraState& cameraState, const BattleVM::Body& body);
};


class BodyRenderer {
    struct TextureGroup {
        std::string name;
        VertexBuffer_3f_1f_2f_2f buffer;
        std::unique_ptr<Texture> texture = nullptr;
    };

    Graphics* graphics_ = nullptr;
    int lastTextureId_ = 0;
    std::unordered_map<int, TextureGroup> textureGroups_ = {};

    VertexBuffer_3f_4f_1f vertexBufferCasualties_;
    VertexBuffer_3f vertexBufferWeapons_;
    VertexBuffer_3f_4f vertexBufferVolley_;
    std::unique_ptr<Pipeline> pipelineBillboards_ = nullptr;
    std::unique_ptr<Pipeline> pipelineCasualties_ = nullptr;
    std::unique_ptr<Pipeline> pipelineWeapons_ = nullptr;
    std::unique_ptr<Pipeline> pipelineVolley_ = nullptr;

public:
    BodyVertices bodyVertices_ = {};

    explicit BodyRenderer(Graphics& graphics);

    std::pair<int, bool> getTexture(const std::string& textureName);
    void setTexture(int textureId, const Image& image);

    void update(const BodyVertices& vertices);
    void render(const Viewport& viewport, const CameraState& cameraState);
};


#endif
