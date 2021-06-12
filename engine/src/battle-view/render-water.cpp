// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-water.h"
#include "graphics/graphics.h"
#include "battle-model/terrain-map.h"


namespace {
    int insideCircle(bounds2f bounds, glm::vec2 p) {
        return glm::distance(p, bounds.mid()) <= bounds.x().size() / 2 ? 1 : 0;
    }

    int insideCircle(bounds2f bounds, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3) {
        return insideCircle(bounds, p1) + insideCircle(bounds, p2) + insideCircle(bounds, p3);
    }

    std::vector<Vertex<_2f>>* chooseShape(int count, std::vector<Vertex<_2f>>* inside, std::vector<Vertex<_2f>>* border) {
        switch (count) {
            case 1:
            case 2:
                return border;

            case 3:
                return inside;

            default:
                return nullptr;
        }
    }

}


void WaterVertices::update(const TerrainMap& terrainMap) {
    bounds = terrainMap.getBounds();

    border.clear();
    inside.clear();

    int n = 64;
    glm::vec2 s = bounds.size() / (float)n;
    for (int x = 0; x < n; ++x)
        for (int y = 0; y < n; ++y) {
            glm::vec2 p = bounds.min + s * glm::vec2(x, y);
            if (terrainMap.containsWater(bounds2f(p, p + s))) {
                glm::vec2 v11 = p;
                glm::vec2 v12 = p + glm::vec2(0, s.y);
                glm::vec2 v21 = p + glm::vec2(s.x, 0);
                glm::vec2 v22 = p + s;

                std::vector<Vertex<_2f>>* s = chooseShape(insideCircle(bounds, v11, v22, v12), &inside, &border);
                if (s) {
                    s->push_back(Vertex<_2f>(v11));
                    s->push_back(Vertex<_2f>(v22));
                    s->push_back(Vertex<_2f>(v12));
                }

                s = chooseShape(insideCircle(bounds, v22, v11, v21), &inside, &border);
                if (s) {
                    s->push_back(Vertex<_2f>(v22));
                    s->push_back(Vertex<_2f>(v11));
                    s->push_back(Vertex<_2f>(v21));
                }
            }
        }

    invalid = false;
}




WaterRenderer::WaterRenderer(Graphics& graphics) :
        vertexBufferInside_{graphics.getGraphicsApi()},
        vertexBufferBorder_{graphics.getGraphicsApi()}
{
    pipelineInside_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<WaterInsideShader>());
    pipelineBorder_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<WaterBorderShader>());
}


void WaterRenderer::update(const WaterVertices& vertices) {
    vertexBufferInside_.updateVBO(vertices.inside);
    vertexBufferBorder_.updateVBO(vertices.border);
    bounds_ = vertices.bounds;
}


void WaterRenderer::render(const Viewport& viewport, const glm::mat4& transform) {
    GL_TRACE(*pipelineInside_).setVertices(GL_TRIANGLES, vertexBufferInside_, {"position"})
            .setUniform("transform", transform)
            .setUniform("map_bounds", glm::vec4(bounds_.min, bounds_.size()))
            .setTexture("texture", nullptr)
            .setDepthTest(true)
            .setDepthMask(true)
            .render(viewport);

    GL_TRACE(*pipelineBorder_).setVertices(GL_TRIANGLES, vertexBufferBorder_, {"position"})
            .setUniform("transform", transform)
            .setUniform("map_bounds", glm::vec4(bounds_.min, bounds_.size()))
            .setTexture("texture", nullptr)
            .setDepthTest(true)
            .setDepthMask(true)
            .render(viewport);
}
