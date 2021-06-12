// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__RENDERBUFFER_H
#define WARSTAGE__GRAPHICS__RENDERBUFFER_H

#include "./graphics-api.h"


class Renderbuffer {
    friend class Framebuffer;

    GraphicsApi& graphicsApi_;
    GLuint id_;

public:
    explicit Renderbuffer(GraphicsApi& graphicsApi);
    Renderbuffer(Renderbuffer&&) noexcept;
    Renderbuffer(const Renderbuffer&) = delete;
    ~Renderbuffer();

    Renderbuffer& operator=(Renderbuffer&&) noexcept;
    Renderbuffer& operator=(const Renderbuffer&) = delete;

    [[nodiscard]] GraphicsApi& getGraphicsApi() const noexcept { return graphicsApi_; }

    void prepareColorBuffer(GLsizei width, GLsizei height);
    void prepareDepthBuffer(GLsizei width, GLsizei height);
};


#endif
