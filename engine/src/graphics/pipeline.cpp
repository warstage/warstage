// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./pipeline.h"
#include "./framebuffer.h"
#include "./viewport.h"
#include "./texture.h"
#include "utilities/logging.h"


PipelineUniformBase::PipelineUniformBase(GraphicsApi& graphicsApi, std::string name, GLint location) :
    graphicsApi_{&graphicsApi},
    name_{std::move(name)},
    location_{location} {
}


void PipelineUniformBase::assign(int value) {
    graphicsApi_->uniform1i(location_, value);
}


void PipelineUniformBase::assign(float value) {
    graphicsApi_->uniform1f(location_, value);
}


void PipelineUniformBase::assign(const glm::vec2& value) {
    graphicsApi_->uniform2f(location_, value);
}


void PipelineUniformBase::assign(const glm::vec3& value) {
    graphicsApi_->uniform3f(location_, value);
}


void PipelineUniformBase::assign(const glm::vec4& value) {
    graphicsApi_->uniform4f(location_, value);
}


void PipelineUniformBase::assign(const glm::mat2& value) {
    graphicsApi_->uniformMatrix2f(location_, value);
}


void PipelineUniformBase::assign(const glm::mat3& value) {
    graphicsApi_->uniformMatrix3f(location_, value);
}


void PipelineUniformBase::assign(const glm::mat4& value) {
    graphicsApi_->uniformMatrix4f(location_, value);
}


/***/


PipelineTexture::PipelineTexture(GraphicsApi& graphicsApi, std::string name, GLint location, GLenum texture) :
    graphicsApi_{&graphicsApi},
    name_{std::move(name)},
    location_{location},
    texture_{texture} {
}


void PipelineTexture::setValue(Texture* value, const Sampler& sampler) {
    value_ = value;
    sampler_ = sampler;
}


void PipelineTexture::assign() {
    if (value_) {
        graphicsApi_->activeTexture(GL_TEXTURE0 + texture_);
        graphicsApi_->bindTexture(GL_TEXTURE_2D, value_->id_);
        graphicsApi_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(sampler_.minFilter));
        graphicsApi_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(sampler_.magFilter));
        graphicsApi_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(sampler_.sAddressMode));
        graphicsApi_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(sampler_.tAddressMode));
    }
    graphicsApi_->uniform1i(location_, (GLint)texture_);
}


/***/


Pipeline::Pipeline(GraphicsApi& graphicsApi) :
        graphicsApi_{&graphicsApi} {
}


Pipeline::Pipeline(const PipelineInitializer& pipelineInitializer) :
        graphicsApi_{&pipelineInitializer.graphicsApi_},
        program_{pipelineInitializer.program_},
        blendSrcFactor_{pipelineInitializer.blendSrcFactor},
        blendDstFactor_{pipelineInitializer.blendDstFactor} {
}


Pipeline::~Pipeline() {
    for (auto uniform : uniforms_)
        delete uniform;
    for (auto texture : textures_)
        delete texture;
}


void Pipeline::render(const Viewport& viewport) {
    bool has_vertices = false;
    if (vertices_) {
        has_vertices = vertices_->vbo_ != 0 && vertices_->count_ != 0;
    }

    auto frameBuffer = viewport.getFrameBuffer();
    if (frameBuffer) {
        graphicsApi_->bindFramebuffer(GL_FRAMEBUFFER, frameBuffer->id_);
#ifdef OPENWAR_PLATFORM_MAC
        if ((_clearBits || has_vertices) && !frameBuffer->HasColor()) {
            _api->DrawBuffer(GL_NONE);
        }
#endif
    }

    bounds2i b = viewport.getViewportBounds();
    graphicsApi_->viewport(b.min.x, b.min.y, b.x().size(), b.y().size());

    if (clearBits_) {
        if (clearBits_ & GL_DEPTH_BUFFER_BIT)
            graphicsApi_->depthMask(GL_TRUE);
        graphicsApi_->clearColor(clearColor_);
        graphicsApi_->clear(clearBits_);
        clearBits_ = 0;
    }

    if (has_vertices) {
        graphicsApi_->useProgram(program_->program_);

#ifdef WARSTAGE_USE_OPENGL32
        gl-BindFragDataLocation(_shaderProgram->_program, 0, "out_FragColor");
        CHECK_OPENGL_ERROR();
    
        gl-BindVertexArray(_shaderProgram->_vao);
        CHECK_OPENGL_ERROR();
#endif

        for (auto uniform : uniforms_)
            uniform->assign();

        for (auto texture : textures_)
            texture->assign();

        graphicsApi_->useProgram(program_->program_);

        if (vertices_->vbo_) {
            graphicsApi_->bindBuffer(GL_ARRAY_BUFFER, vertices_->vbo_);
        }

        for (auto& attribute : attributes_) {
            if (attribute.index_ != -1) {
                auto index = static_cast<GLuint>(attribute.index_);
                graphicsApi_->enableVertexAttribArray(index);

                auto pointer = reinterpret_cast<const GLvoid*>(attribute.offset_);
                graphicsApi_->vertexAttribPointer(index, attribute.size_, attribute.type_, GL_FALSE, attribute.stride_, pointer);
            }
        }

        if (blendSrcFactor_ != GL_ONE || blendDstFactor_ != GL_ZERO) {
            graphicsApi_->enable(GL_BLEND);
            graphicsApi_->blendFunc(blendSrcFactor_, blendDstFactor_);
        }

        if (lineWidth_ != 0.0f)
            graphicsApi_->lineWidth(lineWidth_);

        if (depthTest_)
            graphicsApi_->enable(GL_DEPTH_TEST);
        else
            graphicsApi_->disable(GL_DEPTH_TEST);

        graphicsApi_->depthMask(static_cast<GLboolean>(depthMask_));

        if (cullBack_)
            graphicsApi_->enable(GL_CULL_FACE);
        else
            graphicsApi_->disable(GL_CULL_FACE);

        graphicsApi_->drawArrays(renderMode_, 0, vertices_->count_);

        if (blendSrcFactor_ != GL_ONE || blendDstFactor_ != GL_ZERO) {
            graphicsApi_->disable(GL_BLEND);
            graphicsApi_->blendFunc(GL_ONE, GL_ZERO);
        }

        for (auto& attribute : attributes_) {
            if (attribute.index_ != -1)
                graphicsApi_->disableVertexAttribArray(static_cast<GLuint>(attribute.index_));
        }

        if (vertices_->vbo_)
            graphicsApi_->bindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (frameBuffer) {
#ifdef OPENWAR_PLATFORM_MAC
        if (!frameBuffer->HasColor())
            _api->DrawBuffer(GL_BACK);
#endif
        graphicsApi_->bindFramebuffer(GL_FRAMEBUFFER,  0);
    }
}


PipelineTexture* Pipeline::getTexture(const char* name) {
#ifdef WARSTAGE_USE_OPENGL32
    std::string s;
    if (std::strcmp(name, "texture") == 0) {
        s = "texture_";
        name = s.c_str();
    }
#endif
    
    for (auto texture : textures_)
        if (texture->name_ == name)
            return texture;

    auto location = graphicsApi_->getUniformLocation(program_->program_, name);
    auto result = new PipelineTexture{*graphicsApi_, name, location, (GLenum)textureCount_++};
    textures_.push_back(result);
    return result;
}
