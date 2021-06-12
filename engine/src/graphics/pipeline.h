// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__PIPELINE_H
#define WARSTAGE__GRAPHICS__PIPELINE_H

#include "./graphics-api.h"
#include "./program.h"
#include "./vertex.h"
#include "./vertex-buffer.h"
#include "./sampler.h"

class Framebuffer;
class Texture;
class Viewport;
class Program;
class PipelineInitializer;


struct PipelineAttribute {
    GLint index_;
    GLint size_;
    GLenum type_;
    GLsizei stride_;
    GLintptr offset_;

    PipelineAttribute(GLint index, GLint size, GLenum type, GLsizei stride, GLintptr offset) :
        index_{index}, size_{size}, type_{type}, stride_{stride}, offset_{offset} {
    }
};


class PipelineUniformBase {
    friend class Pipeline;

    GraphicsApi* graphicsApi_{};
    std::string name_{};
    GLint location_{};

protected:
    PipelineUniformBase(GraphicsApi& graphicsApi, std::string name, GLint location);
    virtual ~PipelineUniformBase() = default;

    virtual void assign() = 0;

    void assign(int value);
    void assign(float value);
    void assign(const glm::vec2& value);
    void assign(const glm::vec3& value);
    void assign(const glm::vec4& value);
    void assign(const glm::mat2& value);
    void assign(const glm::mat3& value);
    void assign(const glm::mat4& value);
};


template<class T>
class PipelineUniform : public PipelineUniformBase {
    friend class Pipeline;

    T value_{};

    using PipelineUniformBase::PipelineUniformBase;

    void assign() override {
        PipelineUniformBase::assign(value_);
    }
};


class PipelineTexture {
    friend class Pipeline;

    GraphicsApi* graphicsApi_{};
    std::string name_{};
    GLint location_{};
    GLenum texture_{};
    Texture* value_{};
    Sampler sampler_{};

protected:
    PipelineTexture(GraphicsApi& graphicsApi, std::string name, GLint location, GLenum texture);

    void setValue(Texture* value, const Sampler& sampler);
    void assign();
};


class Pipeline {
protected:
    GraphicsApi* graphicsApi_ = nullptr;
    std::shared_ptr<Program> program_ = nullptr;
    std::vector<PipelineUniformBase*> uniforms_ = {};
    std::vector<PipelineTexture*> textures_ = {};
    std::vector<PipelineAttribute> attributes_ = {};
    GLenum renderMode_ = {};
    GLenum blendSrcFactor_ = GL_ONE;
    GLenum blendDstFactor_ = GL_ZERO;
    VertexBufferBase* vertices_ = nullptr;
    int textureCount_ = 0;
    GLbitfield clearBits_ = {};
    glm::vec4 clearColor_ = {};
    GLfloat lineWidth_ = 0.0f;
    bool depthTest_ = false;
    bool depthMask_ = false;
    bool cullBack_ = false;

public:
    explicit Pipeline(GraphicsApi& graphicsApi);
    explicit Pipeline(const PipelineInitializer& pipelineInitializer);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    void render(const Viewport& viewport);

    template<class T>
    PipelineUniform<T>* getUniform(const char* name) {
        for (auto uniform : uniforms_)
            if (uniform->name_ == name)
                return static_cast<PipelineUniform<T>*>(uniform);

        auto location = graphicsApi_->getUniformLocation(program_->program_, name);
        auto result = new PipelineUniform<T>{*graphicsApi_, name, location};
        uniforms_.push_back(static_cast<PipelineUniformBase*>(result));
        return result;
    }

    PipelineTexture* getTexture(const char* name);

    PipelineAttribute makePipelineAttribute(const VertexAttributeTraits& traits, GLsizei stride) {
        return PipelineAttribute{
                graphicsApi_->getAttribLocation(program_->program_, traits.name),
                traits.size,
                traits.type,
                stride,
                traits.offset
        };
    }

#ifdef ENABLE_GL_TRACE
    Pipeline& trace(const char* file, int line) {
        graphicsApi_->trace(file, line);
        return *this;
    }
#endif

    Pipeline& clearDepth() {
        clearBits_ |= GL_DEPTH_BUFFER_BIT;
        return *this;
    }

    Pipeline& clearColor(const glm::vec4& value) {
        clearBits_ |= GL_COLOR_BUFFER_BIT;
        clearColor_ = value;
        return *this;
    }

    Pipeline& setDepthTest(bool value) {
        depthTest_ = value;
        return *this;
    }

    Pipeline& setDepthMask(bool value) {
        depthMask_ = value;
        return *this;
    }

    Pipeline& setCullBack(bool value) {
        cullBack_ = value;
        return *this;
    }

    template<class T>
    Pipeline& setUniform(const char* name, const T& value) {
        getUniform<T>(name)->value_ = value;
        return *this;
    }

    Pipeline& setTexture(const char* name, Texture* value, const Sampler& sampler = Sampler{}) {
        getTexture(name)->setValue(value, sampler);
        return *this;
    }

    template<typename Vertex>
    Pipeline& setVertices(GLenum renderMode, VertexBuffer<Vertex>& vertices, const std::initializer_list<const char*>& names) {
        assert(std::tuple_size_v<Vertex> == names.size()); // "incorrect number of names"
        renderMode_ = renderMode;
        vertices_ = &vertices;
        attributes_.clear();
        const auto stride = static_cast<GLsizei>(sizeof(Vertex));
        for (const auto& traits : getVertexAttributeTraits<Vertex>(names))
            attributes_.push_back(makePipelineAttribute(traits, stride));
        return *this;
    }

    Pipeline& setLineWidth(GLfloat value) {
        lineWidth_ = value;
        return *this;
    }
};


class PipelineInitializer {
    friend class Pipeline;
    GraphicsApi& graphicsApi_;
    std::shared_ptr<Program> program_;
public:
    GLenum blendSrcFactor = GL_ONE;
    GLenum blendDstFactor = GL_ZERO;
    PipelineInitializer(GraphicsApi& graphicsApi, const char* vertexShaderSource, const char* fragmentShaderSource) :
            graphicsApi_{graphicsApi},
            program_{std::make_shared<Program>(graphicsApi, vertexShaderSource, fragmentShaderSource)} {
    }
};


#endif
