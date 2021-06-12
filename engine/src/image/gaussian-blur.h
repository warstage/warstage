// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__IMAGE__GAUSSIAN_BLUR_H
#define WARSTAGE__IMAGE__GAUSSIAN_BLUR_H

#include <glm/glm.hpp>

// Port of the gaussian blur algorithm by Ivan Kuckir,
// released inther MIT licence. For details, see
// http://blog.ivank.net/fastest-gaussian-blur.html

struct GaussianBlur {
    void apply(glm::vec4* scl, glm::vec4* tcl, int w, int h, float r);
};

#endif
