// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./render-body.h"
#include "./camera-state.h"
#include "graphics/viewport.h"
#include "graphics/pipeline.h"
#include "graphics/texture.h"
#include "graphics/graphics.h"


namespace {
    std::size_t getClosestAngleIndex(const std::vector<float>& angles, float angle) {
        std::size_t index = 0;
        float diff = 360.0f;
        for (auto i = angles.begin(); i != angles.end(); ++i) {
            float d = std::fabs(std::fmodf(*i - angle, 360.0f) - 180.0f);
            if (d < diff) {
                diff = d;
                index = i - angles.begin();
            }
        }
        return index;
    }
}


void BodyVertices::update(const CameraState& cameraState, const BattleVM::Model& model) {
    casualties.clear();
    weapons.clear();
    volleys.clear();
    for (auto& i : billboards) {
        i.second.clear();
    }
    addCasualties(model);
    addWeapons(model);
    addVolleys(model);
    addBillboards(cameraState, model);
}

void BodyVertices::addCasualties(const BattleVM::Model& model) {
    if (model.casualties.empty())
        return;

    glm::vec4 c1 = glm::vec4(1, 1, 1, 0.8);
    glm::vec4 cr = glm::vec4(1, 0, 0, 0);
    glm::vec4 cb = glm::vec4(0, 0, 1, 0);

    for (const auto& casualty : model.casualties) {
        if (casualty.time <= 1) {
            glm::vec4 c2 = glm::vec4{casualty.color, 0.0f};
            glm::vec4 c = glm::mix(c1, c2, casualty.time);
            casualties.emplace_back(casualty.body.state.position, c, 6.0f);
        }
    }
}


void BodyVertices::addWeapons(const BattleVM::Model& model) {
    for (const auto& unitVM : model.units) {
        appendWeapons(*unitVM);
    }
}


void BodyVertices::appendWeapons(const BattleVM::Unit& unitVM) {
    for (const auto& element : unitVM.elements) {
        const auto& body = element.body;
        assert(body.state.lines.size() == body.shape->lines.size());
        std::size_t index = 0;
        for (auto& trajectory : body.shape->lines) {
            const auto& points = body.state.lines[index].points;
            const auto n = trajectory.deltas.size() - 1;
            assert(points.size() == n + 1);
            for (std::size_t i = 0; i < n; ++i) {
                weapons.emplace_back(points[i]);
                weapons.emplace_back(points[i + 1]);
            }
            ++index;
        }
    }
}


void BodyVertices::addVolleys(const BattleVM::Model& model) {
    for (const auto& volley : model.volleys) {
        appendVolley(*volley);
    }
}


void BodyVertices::appendVolley(const BattleVM::Volley& volley) {
    for (const auto& projectile : volley.projectiles) {
        const auto& body = projectile.body;
        if (body.state.invisible) {
            continue;
        }
        assert(body.state.lines.size() == body.shape->lines.size());
        std::size_t index = 0;
        for (auto& trajectory : body.shape->lines) {
            const auto& points = body.state.lines[index].points;
            const auto& colors = trajectory.colors;
            const auto n = trajectory.deltas.size() - 1;
            assert(points.size() == n + 1);
            assert(colors.size() == n + 1);
            for (std::size_t i = 0; i < n; ++i) {
                volleys.emplace_back(points[i], colors[i]);
                volleys.emplace_back(points[i + 1], colors[i + 1]);
            }
            ++index;
        }
    }
}


void BodyVertices::addBillboards(const CameraState& cameraState, const BattleVM::Model& model) {
    for (const auto& vegetation : model.vegetation) {
        addBillboardVertices(cameraState, vegetation.body);
    }

    for (const auto& casualty : model.casualties) {
        addBillboardVertices(cameraState, casualty.body);
    }

    for (const auto& unitVM : model.units) {
        for (const auto& element : unitVM->elements) {
            addBillboardVertices(cameraState, element.body);
        }
    }

    for (const auto& particle : model.particles) {
        addBillboardVertices(cameraState, particle.body);
    }
}


void BodyVertices::addBillboardVertices(const CameraState& cameraState, const BattleVM::Body& body) {
    const auto& shape = *body.shape;
    std::size_t index = 0;
    for (auto& skin : shape.skins) {
        assert(index < body.state.skins.size());
        assert(body.state.skins[index].loop < static_cast<int>(skin.loops.size()));
        const auto& loop = skin.loops[body.state.skins[index].loop];
        const auto facing = glm::degrees(body.state.orientation - cameraState.GetCameraFacing());
        const auto frame = static_cast<int>(std::floorf(body.state.skins[index].frame));
        const auto closestIndex = getClosestAngleIndex(loop.angles, facing);
        const auto v = loop.vertices.begin() + 4 * (loop.angles.size() * frame + closestIndex);
        const float height = body.shape->size.y * body.state.skins[index].scale;
        const auto position = glm::vec3{body.state.position.xy(), body.state.position.z + skin.adjust * height};

        billboards[loop.texture].push_back({position, height, {v[0], v[1]}, {v[2] - v[0], v[3] - v[1]}});
    }
}


BodyRenderer::BodyRenderer(Graphics& graphics) :
        graphics_{&graphics},
        vertexBufferCasualties_{graphics.getGraphicsApi()},
        vertexBufferWeapons_{graphics.getGraphicsApi()},
        vertexBufferVolley_{graphics.getGraphicsApi()} {
    pipelineCasualties_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<BillboardColorShader>());
    pipelineWeapons_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<PlainShader_3f>());
    pipelineVolley_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<GradientShader_3f>());
    pipelineBillboards_ = std::make_unique<Pipeline>(graphics.getPipelineInitializer<BillboardTextureShader>());
}


std::pair<int, bool> BodyRenderer::getTexture(const std::string& textureName) {
    for (const auto& textureGroup : textureGroups_) {
        if (textureGroup.second.name == textureName) {
            return {textureGroup.first, true};
        }
    }
    int textureId = ++lastTextureId_;
    textureGroups_.emplace(textureId, TextureGroup{
            .name = textureName,
            .buffer = VertexBuffer_3f_1f_2f_2f(graphics_->getGraphicsApi()),
    });
    return {textureId, false};
}


void BodyRenderer::setTexture(int textureId, const Image& image) {
    auto i = textureGroups_.find(textureId);
    assert(i != textureGroups_.end());
    auto& texture = i->second.texture;
    texture = std::make_unique<Texture>(graphics_->getGraphicsApi());
    texture->load(image);
}


void BodyRenderer::update(const BodyVertices& vertices) {
    vertexBufferCasualties_.updateVBO(vertices.casualties);
    vertexBufferWeapons_.updateVBO(vertices.weapons);
    vertexBufferVolley_.updateVBO(vertices.volleys);
    for (auto& textureGroup : textureGroups_) {
        auto i = vertices.billboards.find(textureGroup.first);
        if (i != vertices.billboards.end()) {
            textureGroup.second.buffer.updateVBO(i->second);
        } else {
            textureGroup.second.buffer.updateVBO({});
        }
    }
}


void BodyRenderer::render(const Viewport& viewport, const CameraState& cameraState) {
    for (auto& textureGroup : textureGroups_) {
        if (textureGroup.second.texture) {
            pipelineBillboards_->setVertices(GL_POINTS, textureGroup.second.buffer, {"position", "height", "texcoord", "texsize"})
                    .setUniform("transform", cameraState.GetTransform())
                    .setTexture("texture", textureGroup.second.texture.get())
                    .setUniform("upvector", cameraState.GetCameraUpVector())
                    .setUniform("viewport_height", static_cast<float>(viewport.getViewportBounds().y().size()))
                    .setDepthTest(true)
                    .setDepthMask(true)
                    .render(viewport);
        }
    }

    pipelineCasualties_->setVertices(GL_POINTS, vertexBufferCasualties_, {"position", "color", "height"})
            .setUniform("transform", cameraState.GetTransform())
            .setUniform("upvector", cameraState.GetCameraUpVector())
            .setUniform("viewport_height", 0.25f * viewport.getViewportBounds().y().size())
            .setDepthTest(true)
            .render(viewport);

    pipelineWeapons_->setVertices(GL_LINES, vertexBufferWeapons_, {"position"})
            .setUniform("transform", cameraState.GetTransform())
            .setUniform("point_size", 1.0f)
            .setUniform("color", glm::vec4{0.4f, 0.4f, 0.4f, 0.6f})
            .setLineWidth(1.0f)
            .setDepthTest(true)
            .render(viewport);

    pipelineVolley_->setVertices(GL_LINES, vertexBufferVolley_, {"position", "color"})
            .setUniform("transform", cameraState.GetTransform())
            .setUniform("point_size", 1.0f)
            .setDepthTest(true)
            .render(viewport);
}
