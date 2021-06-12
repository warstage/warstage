// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__TEXTURE_H
#define WARSTAGE__GRAPHICS__TEXTURE_H

#include "graphics-api.h"

class Image;


class Texture {
    friend class Graphics;
    friend class Framebuffer;
    friend class PipelineTexture;

    GraphicsApi& graphicsApi_;
    GLuint id_;

public:
    explicit Texture(GraphicsApi& graphicsApi);
    Texture(Texture&&) noexcept;
    Texture(const Texture&) = delete;
    ~Texture();
    Texture& operator=(Texture&&) noexcept;
    Texture& operator=(const Texture&) = delete;

    [[nodiscard]] GraphicsApi& getGraphicsApi() const noexcept { return graphicsApi_; }

    void prepareColorBuffer(GLsizei width, GLsizei height);
    void prepareDepthBuffer(GLsizei width, GLsizei height);

    void load(const Image& image);
    void load(int width, int height, const void* data);

    void generateMipmap();
};


#endif
