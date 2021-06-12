// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__B_SPLINE_PATCH_H
#define WARSTAGE__GEOMETRY__B_SPLINE_PATCH_H

#include "./geometry.h"
#include <vector>


class BSplinePatch {
    glm::ivec2 size_;
    std::vector<float> values_;

public:
    explicit BSplinePatch(glm::ivec2 size);

    [[nodiscard]] glm::ivec2 size() const { return size_; }

    [[nodiscard]] float getHeight(int x, int y) const;
    void setHeight(int x, int y, float value);

    [[nodiscard]] float interpolate(glm::vec2 position) const;
    [[nodiscard]] std::pair<bool, float> intersect(ray r) const;
};


#endif
