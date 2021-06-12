// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_MODEL__HEIGHT_MAP_H
#define WARSTAGE__BATTLE_MODEL__HEIGHT_MAP_H

#include "geometry/geometry.h"
#include <functional>
#include <vector>
#include <glm/glm.hpp>


class HeightMap {
    bounds2f bounds_;
    glm::ivec2 dim_{1, 1};
    std::vector<float> heights_{0.0f};
    std::vector<glm::vec3> normals_{{0.0f, 0.0f, 1.0f}};

public:
    explicit HeightMap(bounds2f bounds);

    [[nodiscard]] bounds2f getBounds() const { return bounds_; }
    [[nodiscard]] glm::ivec2 getDim() const { return dim_; }

    void update(glm::ivec2 dim, const std::function<float(int, int)>& height);

    [[nodiscard]] float getHeight(int x, int y) const;
    [[nodiscard]] glm::vec3 getNormal(int x, int y) const;

    [[nodiscard]] float interpolateHeight(glm::vec2 position) const;
    [[nodiscard]] glm::vec3 getPosition(glm::vec2 p, float h) const { return glm::vec3(p, interpolateHeight(p) + h); }

    [[nodiscard]] std::pair<bool, float> intersect(ray r) const;

private:
    [[nodiscard]] std::pair<bool, float> intersect_(ray r) const;

    void updateHeights_(const std::function<float(int, int)>& height);
    void updateNormals_();
};


#endif
