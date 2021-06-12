// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./texture.h"
#include "./graphics.h"
#include "image/image.h"


Texture::Texture(GraphicsApi& graphicsApi) :
        graphicsApi_{graphicsApi},
        id_{graphicsApi_.createTexture()} {
}


Texture::Texture(Texture&& other) noexcept :
        graphicsApi_{other.graphicsApi_},
        id_{other.id_} {
    other.id_ = 0;
}


Texture::~Texture() {
    if (id_) {
        graphicsApi_.deleteTexture(id_);
    }
}


Texture& Texture::operator=(Texture&& other) noexcept {
    assert(&graphicsApi_ == &other.graphicsApi_);
    std::swap(id_, other.id_);
    return *this;
}


void Texture::prepareColorBuffer(GLsizei width, GLsizei height) {
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, id_);
    GL_TRACE(graphicsApi_).texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GL_TRACE(graphicsApi_).texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL_TRACE(graphicsApi_).texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, 0);
}


void Texture::prepareDepthBuffer(GLsizei width, GLsizei height) {
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, id_);
    GL_TRACE(graphicsApi_).texImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, nullptr);
    GL_TRACE(graphicsApi_).texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL_TRACE(graphicsApi_).texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, 0);
}


void Texture::load(const Image& image) {
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, id_);
    GL_TRACE(graphicsApi_).texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.size_.x, image.size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data_.get());
}


void Texture::load(int width, int height, const void* data) {
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, id_);
    GL_TRACE(graphicsApi_).texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}


void Texture::generateMipmap() {
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, id_);
    GL_TRACE(graphicsApi_).generateMipmap(GL_TEXTURE_2D);
    GL_TRACE(graphicsApi_).texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TRACE(graphicsApi_).bindTexture(GL_TEXTURE_2D, 0);
}
