// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./viewport.h"
#include "./graphics.h"
#include "./framebuffer.h"


Viewport::Viewport(Graphics* graphics, float scaling) :
    graphics_{graphics},
    scaling_{scaling}
{
}


Viewport::~Viewport() {
}


Graphics* Viewport::getGraphics() const {
    return graphics_;
}


Framebuffer* Viewport::getFrameBuffer() const {
    return frameBuffer_;
}


void Viewport::setFrameBuffer(Framebuffer* frameBuffer) {
    frameBuffer_ = frameBuffer;
}


bounds2i Viewport::getViewportBounds() const {
    return bounds_;
}


void Viewport::setViewportBounds(const bounds2i& value) {
    bounds_ = value;
}
