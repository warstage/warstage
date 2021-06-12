// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__RENDER_TERRAIN_H
#define WARSTAGE__BATTLE_VIEW__RENDER_TERRAIN_H

#include "./shaders.h"
#include "geometry/bounds.h"
#include "battle-model/terrain-map.h"
#include "battle-model/height-map.h"

class BattleView;
class Framebuffer;
class Image;
class ObjectClass;
class Renderbuffer;
class Viewport;


struct TerrainVertices {
    bounds2f terrainBounds{};
    std::vector<Vertex<_2f>> shadowVertices_;
    std::vector<Vertex<_3f, _3f>> insideVertices_;
    std::vector<Vertex<_3f, _3f>> borderVertices_;
    std::vector<Vertex<_3f, _1f>> skirtVertices_;
    std::vector<Vertex<_3f>> lineVertices_;
    std::vector<Vertex<_2f, _2f>> hatchingsMasterVertices_;
    std::vector<Vertex<_2f, _2f>> hatchingsResultVertices_;

    void initialize(const TerrainMap& terrainMap, bool showLines);

    void buildTriangles(const TerrainMap& terrainMap);
    void pushTriangle(const bounds2f& bounds, const Vertex<_3f, _3f>& v0, const Vertex<_3f, _3f>& v1, const Vertex<_3f, _3f>& v2);
    std::vector<Vertex<_3f, _3f>>* selectTerrainVertexBuffer(int inside);
    bool updateInsideHeight(const bounds2f& bounds, const TerrainMap& terrainMap);
    bool updateBorderHeight(const bounds2f& bounds, const TerrainMap& terrainMap);

    void initializeSkirt(const TerrainMap& terrainMap);
    bool updateSkirtHeight(const bounds2f& bounds, const HeightMap& heightMap);

    void updateShadowVertices(const TerrainMap& terrainMap);
    void updateHatchingsMasterVertices(ObjectClass& deploymentZones, ObjectId playerAllianceId);
    void updateHatchingsResultVertices();
    void updateLineVertices(const TerrainMap& terrainMap);
    bool updateLineHeights(const bounds2f& bounds, const HeightMap& heightMap);
};


class TerrainRenderer {
    BattleView* battleView_{};
    Graphics* graphics_{};
    int framebufferWidth_{};
    int framebufferHeight_{};

    std::shared_ptr<Framebuffer> sobelFrameBuffer_{};
    std::shared_ptr<Renderbuffer> sobelColorBuffer_{};
    std::shared_ptr<Texture> sobelDepthBuffer_{};
    glm::mat4 sobelTransform_;

    glm::ivec2 hatchingsMasterBufferSize_;
    std::shared_ptr<Texture> hatchingsMasterColorBuffer_{};
    Framebuffer* hatchingsMasterFrameBuffer_{};

    glm::ivec2 hatchingsIntermediateBufferSize_;
    Framebuffer* hatchingsIntermediateFrameBuffer_{};
    std::shared_ptr<Texture> hatchingsIntermediateColorBuffer_{};
    std::shared_ptr<Renderbuffer> hatchingsIntermediateDepthBuffer_{};

    std::unique_ptr<Texture> hatchingsDeployment_{};
    std::unique_ptr<Texture> hatchingsPatternR_{};
    std::unique_ptr<Texture> hatchingsPatternG_{};
    std::unique_ptr<Texture> hatchingsPatternB_{};

    std::unique_ptr<Texture> colormap_{};
    std::unique_ptr<Texture> splatmap_{};

    VertexBuffer_2f shadowBuffer_;
    VertexBuffer_3f_3f insideBuffer_;
    VertexBuffer_3f_3f borderBuffer_;
    VertexBuffer_3f_1f skirtBuffer_;
    VertexBuffer_3f lineBuffer_;
    VertexBuffer_2f_2f hatchingsMasterBuffer_;
    VertexBuffer_2f_2f hatchingsResultBuffer_;

    bounds2f terrainBounds_{};
    bool showLines_{false};
    bounds2f dirtyBounds_{};

    std::unique_ptr<Pipeline> pipelineShadow_{};
    std::unique_ptr<Pipeline> pipelineTerrainInside_{};
    std::unique_ptr<Pipeline> pipelineTerrainBorder_{};
    std::unique_ptr<Pipeline> pipelineTerrainSkirt_{};
    std::unique_ptr<Pipeline> pipelineLines_{};
    std::unique_ptr<Pipeline> pipelineDepthInside_{};
    std::unique_ptr<Pipeline> pipelineDepthBorder_{};
    std::unique_ptr<Pipeline> pipelineDepthSkirt_{};
    std::unique_ptr<Pipeline> pipelineSobelFilter_{};
    std::unique_ptr<Pipeline> pipelineHatchingsMaster_{};
    std::unique_ptr<Pipeline> pipelineHatchingsInside_{};
    std::unique_ptr<Pipeline> pipelineHatchingsBorder_{};
    std::unique_ptr<Pipeline> pipelineHatchingsResult_{};

public:
    TerrainVertices terrainVertices_;

    explicit TerrainRenderer(BattleView* battleView);
    ~TerrainRenderer();

    void initialize();

    void update(const TerrainVertices& vertices);

    void preRenderSobel(const Viewport& viewport, const glm::mat4& transform);
    void renderShadow(const Viewport& viewport, const glm::mat4& transform);
    void renderGround(const Viewport& viewport, const glm::mat4& transform);
    void renderLines(const Viewport& viewport, const glm::mat4& transform);

    void enableSobelBuffers();
    void updateSobelBufferSize(const Viewport& viewport);
    void updateSobelTexture(const glm::mat4& transform);
    void renderSobelTexture(const Viewport& viewport);

    void tryEnableHatchingsBuffers();
    void prepareHatchings();
    void prerenderHatchings1();
    void prerenderHatchings2(const glm::mat4& transform);
    void renderHatchings(const Viewport& viewport);

    void setDirtyBounds(bounds2f bounds);
    void updateChanges(bounds2f bounds);
    void updateSplatmap();

private:
    static std::unique_ptr<Texture> createColorMap(Graphics* graphics);
};


#endif
