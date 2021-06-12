// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-terrain.h"
#include "./battle-view.h"
#include "./camera-state.h"
#include "graphics/framebuffer.h"
#include "graphics/graphics.h"
#include "image/image.h"
#include "graphics/renderbuffer.h"
#include "graphics/viewport.h"
#include "graphics/texture.h"
#include "utilities/logging.h"

#include <cstdlib>
#include <glm/gtc/matrix_transform.hpp>


namespace {
    int insideCircle(bounds2f bounds, glm::vec2 p) {
        return glm::length(p - bounds.mid()) <= bounds.x().size() / 2 ? 1 : 0;
    }

    int insideCircle(bounds2f bounds, glm::vec2 v1, glm::vec2 v2, glm::vec2 v3) {
        return insideCircle(bounds, v1) + insideCircle(bounds, v2) + insideCircle(bounds, v3);
    }
}


void TerrainVertices::initialize(const TerrainMap& terrainMap, bool showLines) {
    initializeSkirt(terrainMap);
    updateShadowVertices(terrainMap);
    if (showLines) {
        updateLineVertices(terrainMap);
    }
    buildTriangles(terrainMap);
}


void TerrainVertices::buildTriangles(const TerrainMap& terrainMap) {
    auto& heightMap = terrainMap.getHeightMap();
    bounds2f bounds = terrainMap.getBounds();
    glm::vec2 corner = bounds.fix<0, 0>();
    glm::vec2 size = bounds.size();

    terrainBounds = terrainMap.getBounds();
    insideVertices_.clear();
    borderVertices_.clear();

    const int nx = heightMap.getDim().x - 1;
    const int ny = heightMap.getDim().y - 1;
    const float kx = heightMap.getDim().x;
    const float ky = heightMap.getDim().y;

    for (int y = 0; y < ny; y += 2) {
        for (int x = 0; x < nx; x += 2) {
            float x0 = corner.x + size.x * (x / kx);
            float x1 = corner.x + size.x * ((x + 1) / kx);
            float x2 = corner.x + size.x * ((x + 2) / kx);
            float y0 = corner.y + size.y * (y / ky);
            float y1 = corner.y + size.y * ((y + 1) / kx);
            float y2 = corner.y + size.y * ((y + 2) / kx);

            float h00 = heightMap.getHeight(x, y);
            float h02 = heightMap.getHeight(x, y + 2);
            float h20 = heightMap.getHeight(x + 2, y);
            float h11 = heightMap.getHeight(x + 1, y + 1);
            float h22 = heightMap.getHeight(x + 2, y + 2);

            glm::vec3 n00 = heightMap.getNormal(x, y);
            glm::vec3 n02 = heightMap.getNormal(x, y + 2);
            glm::vec3 n20 = heightMap.getNormal(x + 2, y);
            glm::vec3 n11 = heightMap.getNormal(x + 1, y + 1);
            glm::vec3 n22 = heightMap.getNormal(x + 2, y + 2);

            Vertex<_3f, _3f> v00 = Vertex<_3f, _3f>(glm::vec3(x0, y0, h00), n00);
            Vertex<_3f, _3f> v02 = Vertex<_3f, _3f>(glm::vec3(x0, y2, h02), n02);
            Vertex<_3f, _3f> v20 = Vertex<_3f, _3f>(glm::vec3(x2, y0, h20), n20);
            Vertex<_3f, _3f> v11 = Vertex<_3f, _3f>(glm::vec3(x1, y1, h11), n11);
            Vertex<_3f, _3f> v22 = Vertex<_3f, _3f>(glm::vec3(x2, y2, h22), n22);

            pushTriangle(bounds, v00, v20, v11);
            pushTriangle(bounds, v20, v22, v11);
            pushTriangle(bounds, v22, v02, v11);
            pushTriangle(bounds, v02, v00, v11);
        }
    }
}


void TerrainVertices::pushTriangle(const bounds2f& bounds, const Vertex<_3f, _3f>& v0, const Vertex<_3f, _3f>& v1, const Vertex<_3f, _3f>& v2) {
    bool inside = insideCircle(bounds,
            std::get<0>(v0).xy(),
            std::get<0>(v1).xy(),
            std::get<0>(v2).xy());
    std::vector<Vertex<_3f, _3f>>* vertices = selectTerrainVertexBuffer(inside);
    if (vertices) {
        vertices->push_back(v0);
        vertices->push_back(v1);
        vertices->push_back(v2);
    }
}


std::vector<Vertex<_3f, _3f>>* TerrainVertices::selectTerrainVertexBuffer(int inside) {
    switch (inside) {
        case 1:
        case 2:
            return &borderVertices_;
        case 3:
            return &insideVertices_;
        default:
            return nullptr;
    }
}


bool TerrainVertices::updateInsideHeight(const bounds2f& bounds, const TerrainMap& terrainMap) {
    const auto& heightMap = terrainMap.getHeightMap();
    bool dirty = false;
    for (Vertex<_3f, _3f>& vertex : insideVertices_) {
        glm::vec2 p = std::get<0>(vertex).xy();
        if (bounds.contains(p)) {
            glm::ivec2 i = terrainMap.toImageCoordinates(p);
            std::get<0>(vertex).z = heightMap.getHeight(i.x, i.y);
            std::get<1>(vertex) = heightMap.getNormal(i.x, i.y);
            dirty = true;
        }
    }
    return dirty;
}


bool TerrainVertices::updateBorderHeight(const bounds2f& bounds, const TerrainMap& terrainMap) {
    const auto& heightMap = terrainMap.getHeightMap();
    bool dirty = false;
    for (Vertex<_3f, _3f>& vertex : borderVertices_) {
        glm::vec2 p = std::get<0>(vertex).xy();
        if (bounds.contains(p)) {
            glm::ivec2 i = terrainMap.toImageCoordinates(p);
            std::get<0>(vertex).z = heightMap.getHeight(i.x, i.y);
            std::get<1>(vertex) = heightMap.getNormal(i.x, i.y);
            dirty = true;
        }
    }
    return dirty;
}


void TerrainVertices::initializeSkirt(const TerrainMap& terrainMap) {
    const auto& heightMap = terrainMap.getHeightMap();
    auto bounds = terrainMap.getBounds();
    glm::vec2 center = bounds.mid();
    float radius = bounds.x().size() / 2;

    skirtVertices_.clear();

    int n = 1024;
    float d = 2 * (float)M_PI / n;
    for (int i = 0; i < n; ++i) {
        float a = d * i;
        glm::vec2 p = center + radius * vector2_from_angle(a);
        float h = fmaxf(0, heightMap.interpolateHeight(p));

        skirtVertices_.push_back({glm::vec3{p, h + 0.5f}, h});
        skirtVertices_.push_back({glm::vec3{p, -2.5f}, h});
    }

    skirtVertices_.push_back(skirtVertices_[0]);
    skirtVertices_.push_back(skirtVertices_[1]);

}


bool TerrainVertices::updateSkirtHeight(const bounds2f& bounds, const HeightMap& heightMap) {
    bool dirty = false;
    for (size_t i = 0; i < skirtVertices_.size(); i += 2) {
        glm::vec2 p = std::get<0>(skirtVertices_[i]).xy();
        if (bounds.contains(p)) {
            float h = fmaxf(0, heightMap.interpolateHeight(p));
            std::get<1>(skirtVertices_[i]) = h;
            std::get<0>(skirtVertices_[i]).z = h;
            dirty = true;
        }
    }
    return dirty;
}


void TerrainVertices::updateShadowVertices(const TerrainMap& terrainMap) {
    bounds2f bounds = terrainMap.getBounds();
    glm::vec2 center = bounds.mid();
    float radius1 = bounds.x().size() / 2;
    float radius2 = radius1 * 1.075f;

    shadowVertices_.clear();

    const int n = 16;
    for (int i = 0; i < n; ++i) {
        float angle1 = static_cast<float>(i) * 2.0f * (float)M_PI / n;
        float angle2 = static_cast<float>(i + 1) * 2.0f * (float)M_PI / n;

        glm::vec2 p1 = center + radius1 * vector2_from_angle(angle1);
        glm::vec2 p2 = center + radius2 * vector2_from_angle(angle1);
        glm::vec2 p3 = center + radius2 * vector2_from_angle(angle2);
        glm::vec2 p4 = center + radius1 * vector2_from_angle(angle2);

        shadowVertices_.emplace_back(p1);
        shadowVertices_.emplace_back(p2);
        shadowVertices_.emplace_back(p3);
        shadowVertices_.emplace_back(p3);
        shadowVertices_.emplace_back(p4);
        shadowVertices_.emplace_back(p1);
    }
}

void TerrainVertices::updateHatchingsMasterVertices(ObjectClass& deploymentZones, ObjectId playerAllianceId) {
    hatchingsMasterVertices_.clear();

    for (const auto& deploymentZone : deploymentZones) {
        float radius = deploymentZone["radius"_float];
        if (radius > 0) {
            auto allianceId = deploymentZone["alliance"_ObjectId];
            auto position = deploymentZone["position"_vec2];
            bounds2f b = bounds2f{position}.add_radius(radius * 64.0f / 60.0f);
            if (allianceId != playerAllianceId) {
                hatchingsMasterVertices_.push_back({{b.min.x, b.min.y}, {0.0f, 0.0f}});
                hatchingsMasterVertices_.push_back({{b.min.x, b.max.y}, {0.0f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.max.y}, {0.5f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.max.y}, {0.5f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.min.y}, {0.5f, 0.0f}});
                hatchingsMasterVertices_.push_back({{b.min.x, b.min.y}, {0.0f, 0.0f}});
            } else {
                hatchingsMasterVertices_.push_back({{b.min.x, b.min.y}, {0.5f, 0.0f}});
                hatchingsMasterVertices_.push_back({{b.min.x, b.max.y}, {0.5f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.max.y}, {1.0f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.max.y}, {1.0f, 0.5f}});
                hatchingsMasterVertices_.push_back({{b.max.x, b.min.y}, {1.0f, 0.0f}});
                hatchingsMasterVertices_.push_back({{b.min.x, b.min.y}, {0.5f, 0.0f}});
            }
        }
    }

}

void TerrainVertices::updateHatchingsResultVertices() {
    hatchingsResultVertices_.clear();
    hatchingsResultVertices_.push_back({{-1, 1}, {0, 1}});
    hatchingsResultVertices_.push_back({{-1, -1}, {0, 0}});
    hatchingsResultVertices_.push_back({{1, 1}, {1, 1}});
    hatchingsResultVertices_.push_back({{1, -1}, {1, 0}});
}

void TerrainVertices::updateLineVertices(const TerrainMap& terrainMap) {
    const auto& heightMap = terrainMap.getHeightMap();
    bounds2f bounds = terrainMap.getBounds();
    glm::vec2 corner = bounds.min;
    glm::vec2 size = bounds.size();

    lineVertices_.clear();
    int nx = heightMap.getDim().x - 1;
    int ny = heightMap.getDim().y - 1;
    float k = heightMap.getDim().x;

    for (int y = 0; y <= ny; y += 2) {
        for (int x = 0; x <= nx; x += 2) {
            float x0 = corner.x + size.x * (x / k);
            float y0 = corner.y + size.y * (y / k);
            float h00 = heightMap.getHeight(x, y);

            float x2, h20;
            if (x != nx) {
                x2 = corner.x + size.x * ((x + 2) / k);
                h20 = heightMap.getHeight(x + 2, y);
                lineVertices_.push_back({{x0, y0, h00}});
                lineVertices_.push_back({{x2, y0, h20}});
            }
            float y2, h02;
            if (y != ny) {
                y2 = corner.y + size.y * ((y + 2) / k);
                h02 = heightMap.getHeight(x, y + 2);
                lineVertices_.push_back({{x0, y0, h00}});
                lineVertices_.push_back({{x0, y2, h02}});
            }

            if (x != nx && y != ny) {
                float x1 = corner.x + size.x * ((x + 1) / k);
                float y1 = corner.y + size.y * ((y + 1) / k);
                float h11 = heightMap.getHeight(x + 1, y + 1);
                float h22 = heightMap.getHeight(x + 2, y + 2);

                lineVertices_.push_back({{x0, y0, h00}});
                lineVertices_.push_back({{x1, y1, h11}});
                lineVertices_.push_back({{x2, y0, h20}});
                lineVertices_.push_back({{x1, y1, h11}});
                lineVertices_.push_back({{x0, y2, h02}});
                lineVertices_.push_back({{x1, y1, h11}});
                lineVertices_.push_back({{x2, y2, h22}});
                lineVertices_.push_back({{x1, y1, h11}});
            }
        }
    }
}

bool TerrainVertices::updateLineHeights(const bounds2f& bounds, const HeightMap& heightMap) {
    bool dirty = false;
    for (Vertex<_3f>& vertex : lineVertices_) {
        glm::vec2 p = std::get<0>(vertex).xy();
        if (bounds.contains(p)) {
            std::get<0>(vertex).z = heightMap.interpolateHeight(p);
            dirty = true;
        }
    }
    return dirty;
}


TerrainRenderer::TerrainRenderer(BattleView* battleView) :
        battleView_{battleView},
        graphics_(battleView->getGraphics()),
        shadowBuffer_{graphics_->getGraphicsApi()},
        insideBuffer_{graphics_->getGraphicsApi()},
        borderBuffer_{graphics_->getGraphicsApi()},
        skirtBuffer_{graphics_->getGraphicsApi()},
        lineBuffer_{graphics_->getGraphicsApi()},
        hatchingsMasterBuffer_{graphics_->getGraphicsApi()},
        hatchingsResultBuffer_{graphics_->getGraphicsApi()}
{
    colormap_ = createColorMap(battleView->getGraphics());
    splatmap_ = std::make_unique<Texture>(battleView->getGraphics()->getGraphicsApi());

    enableSobelBuffers();
}


TerrainRenderer::~TerrainRenderer() {
    delete hatchingsMasterFrameBuffer_;
    delete hatchingsIntermediateFrameBuffer_;
}


void TerrainRenderer::initialize() {
    terrainVertices_.initialize(battleView_->getTerrainMap(), showLines_);

    borderBuffer_.setDirty();
    insideBuffer_.setDirty();
    skirtBuffer_.setDirty();
    shadowBuffer_.setDirty();

    if (showLines_) {
        lineBuffer_.setDirty();
    }
}


void TerrainRenderer::update(const TerrainVertices& vertices) {
    terrainBounds_ = vertices.terrainBounds;

    if (!dirtyBounds_.empty()) {
        updateChanges(dirtyBounds_);
        dirtyBounds_ = bounds2f{};
    }

    bool updated = false;
    if (insideBuffer_.isDirty()) {
        insideBuffer_.updateVBO(vertices.insideVertices_);
        updated = true;
    }
    if (borderBuffer_.isDirty()) {
        borderBuffer_.updateVBO(vertices.borderVertices_);
        updated = true;
    }
    if (skirtBuffer_.isDirty()) {
        skirtBuffer_.updateVBO(vertices.skirtVertices_);
        updated = true;
    }
    if (lineBuffer_.isDirty()) {
        lineBuffer_.updateVBO(vertices.lineVertices_);
        updated = true;
    }
    if (shadowBuffer_.isDirty()) {
        shadowBuffer_.updateVBO(vertices.shadowVertices_);
        updated = true;
    }

    if (updated) {
        updateSplatmap();
        sobelTransform_ = glm::mat4{1};
    }
}


void TerrainRenderer::preRenderSobel(const Viewport& viewport, const glm::mat4& transform) {
    if (sobelFrameBuffer_) {
        updateSobelBufferSize(viewport);
        if (sobelTransform_ != transform) {
            updateSobelTexture(transform);
            sobelTransform_ = transform;
        }
    }
}


void TerrainRenderer::renderShadow(const Viewport& viewport, const glm::mat4& transform) {
    glm::vec4 map_bounds = glm::vec4(terrainBounds_.min, terrainBounds_.size());

    if (!pipelineShadow_)
        pipelineShadow_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<GroundShadowShader>());
    GL_TRACE(*pipelineShadow_).setVertices(GL_TRIANGLES, shadowBuffer_, {"position"})
            .setUniform("transform", transform)
            .setUniform("map_bounds", map_bounds)
            .setCullBack(true)
            .setDepthTest(true)
            .render(viewport);
}


void TerrainRenderer::renderGround(const Viewport& viewport, const glm::mat4& transform) {
    glm::vec2 facing = vector2_from_angle(battleView_->getCameraState().GetCameraFacing() - 2.5f * (float)M_PI_4);
    glm::vec3 lightNormal = glm::normalize(glm::vec3(facing, -1));
    glm::vec4 map_bounds = glm::vec4(terrainBounds_.min, terrainBounds_.size());

    if (!pipelineTerrainInside_)
        pipelineTerrainInside_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<TerrainInsideShader>());
    GL_TRACE(*pipelineTerrainInside_).setVertices(GL_TRIANGLES, insideBuffer_, {"position", "normal"})
            .setUniform("transform", transform)
            .setUniform("light_normal", lightNormal)
            .setUniform("map_bounds", map_bounds)
            .setTexture("colormap", colormap_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
            .setTexture("splatmap", splatmap_.get())
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .render(viewport);

    if (!pipelineTerrainBorder_)
        pipelineTerrainBorder_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<TerrainBorderShader>());
    GL_TRACE(*pipelineTerrainBorder_).setVertices(GL_TRIANGLES, borderBuffer_, {"position", "normal"})
            .setUniform("transform", transform)
            .setUniform("light_normal", lightNormal)
            .setUniform("map_bounds", map_bounds)
            .setTexture("colormap", colormap_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
            .setTexture("splatmap", splatmap_.get())
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .render(viewport);

    if (!pipelineTerrainSkirt_)
        pipelineTerrainSkirt_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<TerrainSkirtShader>());
    GL_TRACE(*pipelineTerrainSkirt_).setVertices(GL_TRIANGLE_STRIP, skirtBuffer_, {"position", "height"})
            .setUniform("transform", transform)
            .setTexture("texture", colormap_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .render(viewport);
}


void TerrainRenderer::renderLines(const Viewport& viewport, const glm::mat4& transform) {
    if (showLines_) {
        if (!pipelineLines_)
            pipelineLines_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<PlainShader_3f>());
        GL_TRACE(*pipelineLines_).setVertices(GL_LINES, lineBuffer_, {"position"})
                .setUniform("transform", transform)
                .setUniform("point_size", 1.0f)
                .setUniform("color", glm::vec4(0, 0, 0, 0.06f))
                .render(viewport);
    }
}


void TerrainRenderer::enableSobelBuffers() {
    sobelFrameBuffer_ = std::make_shared<Framebuffer>(graphics_->getGraphicsApi());

    // _sobelColorBuffer = std::make_shared<RenderBuffer>(_gc->Api());
    sobelDepthBuffer_ = std::make_unique<Texture>(graphics_->getGraphicsApi());

    updateSobelBufferSize(battleView_->getViewport());

    // _sobelFrameBuffer->AttachColor(_sobelColorBuffer);
    sobelFrameBuffer_->attachDepth(sobelDepthBuffer_);
}


void TerrainRenderer::updateSobelBufferSize(const Viewport& viewport) {
    glm::ivec2 size = viewport.getViewportBounds().size();

    if (size.x != framebufferWidth_ || size.y != framebufferHeight_) {
        framebufferWidth_ = size.x;
        framebufferHeight_ = size.y;

        if (sobelColorBuffer_) {
            sobelColorBuffer_->prepareColorBuffer(framebufferWidth_, framebufferHeight_);
        }
        if (sobelDepthBuffer_) {
            sobelDepthBuffer_->prepareDepthBuffer(framebufferWidth_, framebufferHeight_);
        }
        sobelTransform_ = glm::mat4{1};
    }
}


void TerrainRenderer::updateSobelTexture(const glm::mat4& transform) {
    glm::vec4 map_bounds = glm::vec4(terrainBounds_.min, terrainBounds_.size());

    Viewport sobelViewport{graphics_, 1.0f};

    sobelViewport.setViewportBounds(bounds2i{0, 0, framebufferWidth_, framebufferHeight_});
    sobelViewport.setFrameBuffer(sobelFrameBuffer_.get());

    if (!pipelineDepthInside_)
        pipelineDepthInside_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<DepthInsideShader>());
    GL_TRACE(*pipelineDepthInside_).setVertices(GL_TRIANGLES, insideBuffer_, {"position", "normal"})
            .setUniform("transform", transform)
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .clearDepth()
            .render(sobelViewport);

    if (!pipelineDepthBorder_)
        pipelineDepthBorder_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<DepthBorderShader>());
    GL_TRACE(*pipelineDepthBorder_).setVertices(GL_TRIANGLES, borderBuffer_, {"position", "normal"})
            .setUniform("transform", transform)
            .setUniform("map_bounds", map_bounds)
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .render(sobelViewport);

    if (!pipelineDepthSkirt_)
        pipelineDepthSkirt_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<DepthSkirtShader>());
    GL_TRACE(*pipelineDepthSkirt_).setVertices(GL_TRIANGLE_STRIP, skirtBuffer_, {"position", "height"})
            .setUniform("transform", transform)
            .setDepthTest(true)
            .setDepthMask(true)
            .setCullBack(true)
            .render(sobelViewport);
}


void TerrainRenderer::renderSobelTexture(const Viewport& viewport) {
    if (sobelDepthBuffer_) {
        std::vector<Vertex<_2f, _2f>> vertices;
        vertices.push_back({{-1, 1}, {0, 1}});
        vertices.push_back({{-1, -1}, {0, 0}});
        vertices.push_back({{1, 1}, {1, 1}});
        vertices.push_back({{1, -1}, {1, 0}});

        VertexBuffer_2f_2f buffer{graphics_->getGraphicsApi()};
        buffer.updateVBO(vertices);

        if (!pipelineSobelFilter_)
            pipelineSobelFilter_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<SobelFilterShader>());
        GL_TRACE(*pipelineSobelFilter_).setVertices(GL_TRIANGLE_STRIP, buffer, {"position", "texcoord"})
                .setUniform("transform", glm::mat4{1})
                .setTexture("depth", sobelDepthBuffer_.get(), Sampler(SamplerMinMagFilter::Nearest, SamplerAddressMode::Clamp))
                .render(viewport);
    }
}


void TerrainRenderer::tryEnableHatchingsBuffers() {
    if (!hatchingsMasterFrameBuffer_) {
        // Hatchings Master Buffer

        hatchingsMasterBufferSize_ = glm::ivec2{128, 128};

        hatchingsMasterColorBuffer_ = std::make_unique<Texture>(graphics_->getGraphicsApi());
        hatchingsMasterColorBuffer_->prepareColorBuffer(hatchingsMasterBufferSize_.x, hatchingsMasterBufferSize_.y);

        hatchingsMasterFrameBuffer_ = new Framebuffer(graphics_->getGraphicsApi());
        hatchingsMasterFrameBuffer_->attachColor(hatchingsMasterColorBuffer_);

        // Hatchings Intermediate Buffer

        hatchingsIntermediateBufferSize_ = glm::ivec2{128, 128};

        hatchingsIntermediateColorBuffer_ = std::make_unique<Texture>(graphics_->getGraphicsApi());
        hatchingsIntermediateDepthBuffer_ = std::make_shared<Renderbuffer>(graphics_->getGraphicsApi());

        hatchingsIntermediateColorBuffer_->prepareColorBuffer(hatchingsIntermediateBufferSize_.x, hatchingsIntermediateBufferSize_.y);
        hatchingsIntermediateDepthBuffer_->prepareDepthBuffer(hatchingsIntermediateBufferSize_.x, hatchingsIntermediateBufferSize_.y);

        hatchingsIntermediateFrameBuffer_ = new Framebuffer(graphics_->getGraphicsApi());
        hatchingsIntermediateFrameBuffer_->attachColor(hatchingsIntermediateColorBuffer_);
        hatchingsIntermediateFrameBuffer_->attachDepth(hatchingsIntermediateDepthBuffer_);

        terrainVertices_.updateHatchingsResultVertices();
        hatchingsResultBuffer_.updateVBO(terrainVertices_.hatchingsResultVertices_);

        // Hacthing Patterns

        const std::uint32_t r = 0xff000000;
        const std::uint32_t g = 0x00ff0000;
        const std::uint32_t b = 0x0000ff00;

        std::array<std::uint32_t, 64> patternD{
                g, g, g, g, r, r, r, r,
                g, g, g, g, r, r, r, r,
                g, g, g, g, r, r, r, r,
                g, g, g, g, r, r, r, r,
                b, b, b, b, 0, 0, 0, 0,
                b, b, b, b, 0, 0, 0, 0,
                b, b, b, b, 0, 0, 0, 0,
                b, b, b, b, 0, 0, 0, 0,
        };
        std::array<std::uint32_t, 64> patternR{
            r, 0, 0, 0, 0, 0, 0, 0,
                0, r, 0, 0, 0, 0, 0, 0,
                0, 0, r, 0, 0, 0, 0, 0,
                0, 0, 0, r, 0, 0, 0, 0,
                0, 0, 0, 0, r, 0, 0, 0,
                0, 0, 0, 0, 0, r, 0, 0,
                0, 0, 0, 0, 0, 0, r, 0,
                0, 0, 0, 0, 0, 0, 0, r,
        };
        std::array<std::uint32_t, 64> patternG{
                g, 0, 0, 0, 0, 0, 0, 0,
                0, g, 0, 0, 0, 0, 0, 0,
                0, 0, g, 0, 0, 0, 0, 0,
                0, 0, 0, g, 0, 0, 0, 0,
                0, 0, 0, 0, g, 0, 0, 0,
                0, 0, 0, 0, 0, g, 0, 0,
                0, 0, 0, 0, 0, 0, g, 0,
                0, 0, 0, 0, 0, 0, 0, g,
        };
        std::array<std::uint32_t, 64> patternB{
                b, 0, 0, 0, 0, 0, 0, 0,
                0, b, 0, 0, 0, 0, 0, 0,
                0, 0, b, 0, 0, 0, 0, 0,
                0, 0, 0, b, 0, 0, 0, 0,
                0, 0, 0, 0, b, 0, 0, 0,
                0, 0, 0, 0, 0, b, 0, 0,
                0, 0, 0, 0, 0, 0, b, 0,
                0, 0, 0, 0, 0, 0, 0, b,
        };


        hatchingsDeployment_ = std::make_unique<Texture>(graphics_->getGraphicsApi());
        hatchingsPatternR_ = std::make_unique<Texture>(graphics_->getGraphicsApi());
        hatchingsPatternG_ = std::make_unique<Texture>(graphics_->getGraphicsApi());
        hatchingsPatternB_ = std::make_unique<Texture>(graphics_->getGraphicsApi());

        hatchingsDeployment_->load(8, 8, patternD.data());
        hatchingsPatternR_->load(8, 8, patternR.data());
        hatchingsPatternG_->load(8, 8, patternG.data());
        hatchingsPatternB_->load(8, 8, patternB.data());
    }
}


void TerrainRenderer::prepareHatchings() {
    auto& deploymentZones = battleView_->battleFederate_->getObjectClass("DeploymentZone");
    if (!hatchingsMasterFrameBuffer_ && deploymentZones.end() != deploymentZones.begin()) {
        tryEnableHatchingsBuffers();
    }

    if (hatchingsMasterFrameBuffer_) {
        terrainVertices_.updateHatchingsMasterVertices(deploymentZones, battleView_->getAllianceId_());
        hatchingsMasterBuffer_.updateVBO(terrainVertices_.hatchingsMasterVertices_);
    }
}


void TerrainRenderer::prerenderHatchings1() {
    if (hatchingsMasterFrameBuffer_) {
        glm::vec3 translate = glm::vec3{-terrainBounds_.mid(), 0};
        glm::vec3 scale = glm::vec3{2.0f / terrainBounds_.size(), 0};

        Viewport masterViewport{graphics_, 1.0f};
        masterViewport.setViewportBounds(bounds2i{0, 0, hatchingsMasterBufferSize_.x, hatchingsMasterBufferSize_.y});
        masterViewport.setFrameBuffer(hatchingsMasterFrameBuffer_);

        if (!pipelineHatchingsMaster_)
            pipelineHatchingsMaster_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<HatchingsMasterShader>());
        GL_TRACE(*pipelineHatchingsMaster_).setVertices(GL_TRIANGLES, hatchingsMasterBuffer_, {"position", "texcoord"})
                .setUniform("transform", glm::translate(glm::scale(glm::mat4{1}, scale), translate))
                .setTexture("texture", hatchingsDeployment_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
                .clearColor(glm::vec4{})
                .render(masterViewport);
    }
}


void TerrainRenderer::prerenderHatchings2(const glm::mat4& transform) {
    if (hatchingsMasterFrameBuffer_) {
        Viewport intermediateViewport{graphics_, 1.0f};
        intermediateViewport.setViewportBounds(bounds2i{0, 0, hatchingsIntermediateBufferSize_.x, hatchingsIntermediateBufferSize_.y});
        intermediateViewport.setFrameBuffer(hatchingsIntermediateFrameBuffer_);

        glm::vec4 map_bounds = glm::vec4{terrainBounds_.min, terrainBounds_.size()};

        if (!pipelineHatchingsInside_)
            pipelineHatchingsInside_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<HatchingsInsideShader>());
        GL_TRACE(*pipelineHatchingsInside_).setVertices(GL_TRIANGLES, insideBuffer_, {"position", nullptr})
                .setUniform("transform", transform)
                .setUniform("map_bounds", map_bounds)
                .setTexture("texture", hatchingsMasterColorBuffer_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
                .setDepthTest(true)
                .setDepthMask(true)
                .setCullBack(true)
                .clearDepth()
                .clearColor(glm::vec4{0, 0, 0, 1})
                .render(intermediateViewport);

        if (!pipelineHatchingsBorder_)
            pipelineHatchingsBorder_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<HatchingsBorderShader>());
        GL_TRACE(*pipelineHatchingsBorder_).setVertices(GL_TRIANGLES, borderBuffer_, {"position", nullptr})
                .setUniform("transform", transform)
                .setUniform("map_bounds", map_bounds)
                .setTexture("texture", hatchingsMasterColorBuffer_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
                .setDepthTest(true)
                .setDepthMask(true)
                .setCullBack(true)
                .render(intermediateViewport);
    }
}


void TerrainRenderer::renderHatchings(const Viewport& viewport) {
    if (hatchingsMasterFrameBuffer_) {
        if (!pipelineHatchingsResult_)
            pipelineHatchingsResult_ = std::make_unique<Pipeline>(graphics_->getPipelineInitializer<HatchingsResultShader>());
        GL_TRACE(*pipelineHatchingsResult_).setVertices(GL_TRIANGLE_STRIP, hatchingsResultBuffer_, {"position", "texcoord"})
                .setUniform("transform", glm::mat4{1})
                .setTexture("texture", hatchingsIntermediateColorBuffer_.get(), Sampler(SamplerMinMagFilter::Linear, SamplerAddressMode::Clamp))
                .setTexture("hatch_r", hatchingsPatternR_.get(), Sampler(SamplerMinMagFilter::Nearest, SamplerAddressMode::Repeat))
                .setTexture("hatch_g", hatchingsPatternG_.get(), Sampler(SamplerMinMagFilter::Nearest, SamplerAddressMode::Repeat))
                .setTexture("hatch_b", hatchingsPatternB_.get(), Sampler(SamplerMinMagFilter::Nearest, SamplerAddressMode::Repeat))
                .setUniform("hatch_scale", viewport.getScaling() * 16.0f)
                .render(viewport);
    }
}


void TerrainRenderer::updateSplatmap() {
    auto& terrainMap = battleView_->getTerrainMap();

    int width = 256; // terrainMap.getImage()->size_.x;
    int height = 256; // terrainMap.getImage()->size_.y;

    GLubyte* data = new GLubyte[4 * width * height];
    GLubyte* p = data;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int forest = terrainMap.getForestValue(x, y);
            float block = terrainMap.getImpassableValue(x, y);
            *p++ = static_cast<GLubyte>(255.0f * block);
            *p++ = static_cast<GLubyte>(forest);
            *p++ = static_cast<GLubyte>(forest);
            *p++ = static_cast<GLubyte>(forest);
        }
    }

    splatmap_->load(width, height, data);
    splatmap_->generateMipmap();

    delete[] data;
}


void TerrainRenderer::setDirtyBounds(bounds2f bounds) {
    if (dirtyBounds_.empty()) {
        dirtyBounds_ = bounds;
    } else {
        dirtyBounds_ = bounds2f {
                glm::min(dirtyBounds_.min, bounds.min),
                glm::max(dirtyBounds_.max, bounds.max),
        };
    }
}


void TerrainRenderer::updateChanges(bounds2f bounds) {
    auto& terrainMap = battleView_->getTerrainMap();
    auto& heightMap = battleView_->getHeightMap();

    terrainVertices_.initializeSkirt(battleView_->getTerrainMap());
    skirtBuffer_.setDirty();

    // inside
    if (terrainVertices_.updateInsideHeight(bounds, terrainMap)) {
        insideBuffer_.setDirty();
    }

    // border
    if (terrainVertices_.updateBorderHeight(bounds, terrainMap)) {
        borderBuffer_.setDirty();
    }

    // lines
    if (showLines_) {
        if (terrainVertices_.updateLineHeights(bounds, heightMap)) {
            lineBuffer_.setDirty();
        }
    }

    // skirt
    if (terrainVertices_.updateSkirtHeight(bounds, heightMap)) {
        skirtBuffer_.setDirty();
    }
}


static int _colorScheme = 0;


static glm::vec3 heightcolor(float h) {
    static std::vector<std::pair<float, glm::vec3>>* _colors = nullptr;
    if (_colors == nullptr) {
        _colors = new std::vector<std::pair<float, glm::vec3>>();
        switch (_colorScheme) {
            case 1:
                _colors->push_back(std::pair<float, glm::vec3>(-2.5, glm::vec3(164, 146, 124) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(-0.5, glm::vec3(219, 186, 153) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(0.0, glm::vec3(191, 171, 129) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(6.5, glm::vec3(114, 150, 65) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(7.0, glm::vec3(120, 150, 64) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(10, glm::vec3(135, 149, 60) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(50, glm::vec3(132, 137, 11) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(150, glm::vec3(132, 137, 11) / 255.0f));
                break;

            case 2: // Granicus
                _colors->push_back(std::pair<float, glm::vec3>(-2.5, glm::vec3(156, 137, 116) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(0.5, glm::vec3(156, 137, 116) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(1.0, glm::vec3(128, 137, 74) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(50, glm::vec3(72, 67, 38) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(150, glm::vec3(72, 67, 38) / 255.0f));
                break;

            case 3: // Issus
                _colors->push_back(std::pair<float, glm::vec3>(-2.5, glm::vec3(204, 168, 146) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(0.5, glm::vec3(204, 168, 146) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(1.0, glm::vec3(221, 138, 88) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(50, glm::vec3(197, 111, 60) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(150, glm::vec3(197, 111, 60) / 255.0f));
                break;

            case 4: // Hydaspes
                _colors->push_back(std::pair<float, glm::vec3>(-2.5, glm::vec3(138, 153, 105) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(0.5, glm::vec3(144, 149, 110) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(1.0, glm::vec3(128, 137, 74) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(50, glm::vec3(72, 67, 38) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(150, glm::vec3(72, 67, 38) / 255.0f));
                break;

            default: // samurai
                _colors->push_back(std::pair<float, glm::vec3>(-2.5, glm::vec3(164, 146, 124) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(-0.5, glm::vec3(219, 186, 153) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(0.0, glm::vec3(194, 142, 102) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(6.5, glm::vec3(199, 172, 148) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(7.0, glm::vec3(177, 172, 132) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(10, glm::vec3(125, 171, 142) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(50, glm::vec3(59, 137, 11) / 255.0f));
                _colors->push_back(std::pair<float, glm::vec3>(150, glm::vec3(59, 137, 11) / 255.0f));
                break;
        }
    }

    int i = 0;
    while (i + 1 < (int)_colors->size() && h > (*_colors)[i + 1].first)
        ++i;

    float h1 = (*_colors)[i].first;
    float h2 = (*_colors)[i + 1].first;
    glm::vec3 c1 = (*_colors)[i].second;
    glm::vec3 c2 = (*_colors)[i + 1].second;

    return glm::mix(c1, c2, (bounds1f(h1, h2).clamp(h) - h1) / (h2 - h1));
}


static glm::vec3 adjust_brightness(glm::vec3 c, float brightness) {
    if (brightness < 0.5f)
        return c * (1.0f - 0.2f * (0.5f - brightness));
    else if (brightness > 0.83f)
        return glm::mix(c, glm::vec3(1.0f), 0.3f * (brightness - 0.5f));
    else
        return glm::mix(c, glm::vec3(1.0f), 0.2f * (brightness - 0.5f));
}


std::unique_ptr<Texture> TerrainRenderer::createColorMap(Graphics* graphics) {
    static Image* image = nullptr;
    if (image == nullptr) {
        static glm::vec3 r[256];
        for (int i = 0; i < 256; ++i) {
            r[i].r = (std::rand() & 0x7fff) / (float)0x7fff;
            r[i].g = (std::rand() & 0x7fff) / (float)0x7fff;
            r[i].b = (std::rand() & 0x7fff) / (float)0x7fff;
        }

        image = new Image{glm::ivec3{64, 256, 4}};
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 64; ++x) {
                float brightness = x / 63.0f;
                float h = -2.5f + 0.5f * y;
                glm::vec3 c = heightcolor(h);
                c = adjust_brightness(c, brightness);
                if (h > 0)
                    c = glm::mix(c, r[y], 0.015f);

                image->setPixel(x, 255 - y, glm::vec4(c, 1));
            }
    }

    auto result = std::make_unique<Texture>(graphics->getGraphicsApi());
    result->load(*image);
    return result;
}
