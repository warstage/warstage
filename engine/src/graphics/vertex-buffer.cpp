// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./vertex-buffer.h"
#include "./program.h"


VertexBufferBase::VertexBufferBase(GraphicsApi& graphicsApi) :
        graphicsApi_{graphicsApi} {
}

VertexBufferBase::VertexBufferBase(VertexBufferBase&& other) noexcept :
        graphicsApi_{other.graphicsApi_},
        vbo_{other.vbo_},
        count_{other.count_},
        dirty_{other.dirty_} {
    other.vbo_ = 0;
    other.count_ = 0;
    other.dirty_ = false;
}


VertexBufferBase::~VertexBufferBase() {
    if (vbo_) {
        graphicsApi_.deleteBuffer(vbo_);
    }
}


VertexBufferBase& VertexBufferBase::operator=(VertexBufferBase&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(vbo_, other.vbo_);
    std::swap(count_, other.count_);
    std::swap(dirty_, other.dirty_);
    return *this;
}
