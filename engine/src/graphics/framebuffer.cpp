// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./framebuffer.h"
#include "./renderbuffer.h"
#include "./texture.h"


Framebuffer::Framebuffer(GraphicsApi& graphicsApi) :
        graphicsApi_{graphicsApi},
        id_{graphicsApi.createFramebuffer()} {
}


Framebuffer::Framebuffer(Framebuffer&& other) noexcept :
        graphicsApi_{other.graphicsApi_},
        id_{other.id_} {
    other.id_ = 0;
}


Framebuffer::~Framebuffer() {
    if (id_) {
        graphicsApi_.deleteFramebuffer(id_);
    }
}


Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(id_, other.id_);
    return *this;
}


void Framebuffer::attachColor(std::shared_ptr<Renderbuffer> value) {
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, id_);
    graphicsApi_.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, value->id_);
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, 0);
    colorRenderbuffer = std::move(value);
}


void Framebuffer::attachColor(std::shared_ptr<Texture> value) {
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, id_);
    graphicsApi_.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, value->id_, 0);
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, 0);
    colorTexture_ = std::move(value);
}


void Framebuffer::attachDepth(std::shared_ptr<Renderbuffer> value) {
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, id_);
    graphicsApi_.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, value->id_);
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, 0);
    depthRenderbuffer = std::move(value);
}


void Framebuffer::attachDepth(std::shared_ptr<Texture> value) {
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, id_);
    graphicsApi_.framebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, value->id_, 0);
    graphicsApi_.bindFramebuffer(GL_FRAMEBUFFER, 0);
    depthTexture_ = std::move(value);
}
