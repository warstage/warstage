// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__VIEWPORT_H
#define WARSTAGE__GRAPHICS__VIEWPORT_H

#include "geometry/bounds.h"


class Framebuffer;
class Graphics;


class Viewport {
    Graphics* graphics_{};
    float scaling_{};
    bounds2i bounds_{};
    Framebuffer* frameBuffer_{};

public:
    Viewport(Graphics* graphics, float scaling);
    virtual ~Viewport();

    [[nodiscard]] float getScaling() const { return scaling_; }

    [[nodiscard]] Graphics* getGraphics() const;

    [[nodiscard]] Framebuffer* getFrameBuffer() const;
    void setFrameBuffer(Framebuffer* frameBuffer);

    [[nodiscard]] bounds2i getViewportBounds() const;
    void setViewportBounds(const bounds2i& value);
};


#endif
