// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__FRAMEBUFFER_H
#define WARSTAGE__GRAPHICS__FRAMEBUFFER_H

#include "./graphics-api.h"


struct Renderbuffer;
class Texture;


class Framebuffer {
    friend class Image;
    friend class Pipeline;

    GraphicsApi& graphicsApi_;
    GLuint id_;
    std::shared_ptr<Renderbuffer> colorRenderbuffer = {};
    std::shared_ptr<Texture> colorTexture_ = {};
    std::shared_ptr<Renderbuffer> depthRenderbuffer = {};
    std::shared_ptr<Texture> depthTexture_ = {};

public:
    explicit Framebuffer(GraphicsApi& graphicsApi);
    Framebuffer(Framebuffer&&) noexcept;
    Framebuffer(const Framebuffer&) = delete;
    ~Framebuffer();

    Framebuffer& operator=(Framebuffer&&) noexcept;
    Framebuffer& operator=(const Framebuffer&) = delete;

    [[nodiscard]] GraphicsApi& getGraphicsApi() const noexcept { return graphicsApi_; }

    void attachColor(std::shared_ptr<Renderbuffer> value);
    void attachColor(std::shared_ptr<Texture> value);

    void attachDepth(std::shared_ptr<Renderbuffer> value);
    void attachDepth(std::shared_ptr<Texture> value);
};


#endif
