// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./renderbuffer.h"


Renderbuffer::Renderbuffer(GraphicsApi& graphicsApi) :
        graphicsApi_{graphicsApi},
        id_{graphicsApi.createRenderbuffer()} {
}

Renderbuffer::Renderbuffer(Renderbuffer&& other) noexcept :
        graphicsApi_{other.graphicsApi_},
        id_{other.id_} {
    other.id_ = 0;
}

Renderbuffer::~Renderbuffer() {
    if (id_) {
        graphicsApi_.deleteRenderbuffer(id_);
    }
}


Renderbuffer& Renderbuffer::operator=(Renderbuffer&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(id_, other.id_);
    return *this;
}


void Renderbuffer::prepareColorBuffer(GLsizei width, GLsizei height) {
    graphicsApi_.bindRenderbuffer(GL_RENDERBUFFER, id_);
    graphicsApi_.renderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width, height);
    graphicsApi_.bindRenderbuffer(GL_RENDERBUFFER, 0);
}


void Renderbuffer::prepareDepthBuffer(GLsizei width, GLsizei height) {
    graphicsApi_.bindRenderbuffer(GL_RENDERBUFFER, id_);
    graphicsApi_.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    graphicsApi_.bindRenderbuffer(GL_RENDERBUFFER, 0);
}
