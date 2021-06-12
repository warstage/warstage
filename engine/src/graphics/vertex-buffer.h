// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__VERTEX_BUFFER_H
#define WARSTAGE__GRAPHICS__VERTEX_BUFFER_H

#include "./graphics-api.h"
#include "./vertex.h"
#include "geometry/bounds.h"


class VertexBufferBase {
    friend class Pipeline;
protected:
    GraphicsApi& graphicsApi_;
    GLuint vbo_{};
    GLsizei count_{};
    bool dirty_{};

public:
    explicit VertexBufferBase(GraphicsApi& graphicsApi);
    VertexBufferBase(VertexBufferBase&& other) noexcept;
    VertexBufferBase(const VertexBufferBase&) = delete;
    virtual ~VertexBufferBase();

    VertexBufferBase& operator=(VertexBufferBase&&) noexcept;
    VertexBufferBase& operator=(const VertexBufferBase&) = delete;

    [[nodiscard]] bool isDirty() const { return dirty_; }
    void setDirty() { dirty_ = true; }
};


template<class VertexType>
class VertexBuffer : public VertexBufferBase {
public:
    typedef VertexType VertexT;

    using VertexBufferBase::VertexBufferBase;

    void updateVBO(const std::vector<VertexT>& vertices) {
        if (!vbo_) {
            vbo_ = graphicsApi_.createBuffer();
            if (!vbo_)
                return;
        }

        if (auto size = static_cast<GLsizeiptr>(sizeof(VertexT) * vertices.size())) {
            auto data = static_cast<const GLvoid*>(vertices.data());
            graphicsApi_.bindBuffer(GL_ARRAY_BUFFER, vbo_);
            graphicsApi_.bufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
            graphicsApi_.bindBuffer(GL_ARRAY_BUFFER, 0);
        }

        count_ = (GLsizei)vertices.size();
        dirty_ = false;
    }
};

using VertexBuffer_2f = VertexBuffer<Vertex<_2f>>;
using VertexBuffer_3f = VertexBuffer<Vertex<_3f>>;
using VertexBuffer_2f_2f = VertexBuffer<Vertex<_2f, _2f>>;

using VertexBuffer_2f_4f = VertexBuffer<Vertex<_2f, _4f>>;
using VertexBuffer_3f_1f = VertexBuffer<Vertex<_3f, _1f>>;
using VertexBuffer_3f_2f = VertexBuffer<Vertex<_3f, _2f>>;
using VertexBuffer_3f_3f = VertexBuffer<Vertex<_3f, _3f>>;
using VertexBuffer_3f_4f = VertexBuffer<Vertex<_3f, _4f>>;

using VertexBuffer_3f_4f_1f = VertexBuffer<Vertex<_3f, _4f, _1f>>;
using VertexBuffer_2f_2f_2f = VertexBuffer<Vertex<_2f, _2f, _2f>>;

using VertexBuffer_3f_1f_2f_2f = VertexBuffer<Vertex<_3f, _1f, _2f, _2f>>;
using VertexBuffer_3f_1f_2f_2f_4f = VertexBuffer<Vertex<_3f, _1f, _2f, _2f, _4f>>;

#endif
