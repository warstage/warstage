// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./b-spline-patch.h"
#include "./b-spline.h"


BSplinePatch::BSplinePatch(glm::ivec2 size) :
    size_{size},
    values_(size.x * size.y, 0.0f) {
}


float BSplinePatch::getHeight(int x, int y) const {
    if (0 <= x && x < size_.x && 0 <= y && y < size_.y)
        return values_[x + size_.x * y];
    return 0;
}


void BSplinePatch::setHeight(int x, int y, float value) {
    if (0 <= x && x < size_.x && 0 <= y && y < size_.y)
        values_[x + size_.x * y] = value;
}


float BSplinePatch::interpolate(glm::vec2 position) const {
    int x = (int)glm::floor(position.x);
    int y = (int)glm::floor(position.y);

    glm::mat4 p(
            getHeight(x - 1, y - 1), getHeight(x + 0, y - 1), getHeight(x + 1, y - 1), getHeight(x + 2, y - 1),
            getHeight(x - 1, y + 0), getHeight(x + 0, y + 0), getHeight(x + 1, y + 0), getHeight(x + 2, y + 0),
            getHeight(x - 1, y + 1), getHeight(x + 0, y + 1), getHeight(x + 1, y + 1), getHeight(x + 2, y + 1),
            getHeight(x - 1, y + 2), getHeight(x + 0, y + 2), getHeight(x + 1, y + 2), getHeight(x + 2, y + 2)
    );

    glm::vec2 t = position - glm::vec2(x, y);

    return BSpline::interpolate(p, t);
}


namespace {
    bool almostZero(float value) {
        static const float epsilon = 10 * std::numeric_limits<float>::epsilon();
        return fabsf(value) < epsilon;
    }
}


std::pair<bool, float> BSplinePatch::intersect(ray r) const {
    bounds1f height = bounds1f(-100, 1000); //min(m), max(m));
    bounds2f bounds(0, 0, size_.x - 1, size_.y - 1);
    bounds2f quad(-0.01f, -0.01f, 1.01f, 1.01f);

    std::pair<bool, float> d = ::intersect(r, bounds3f(bounds, height));
    if (!d.first)
        return d;

    glm::vec3 p = r.point(d.second);

    bounds2f bounds_2(0, 0, size_.x - 2, size_.y - 2);

    int x = (int)bounds_2.x().clamp(p.x);
    int y = (int)bounds_2.y().clamp(p.y);
    int flipX = r.direction.x < 0 ? 0 : 1;
    int flipY = r.direction.y < 0 ? 0 : 1;
    int dx = r.direction.x < 0 ? -1 : 1;
    int dy = r.direction.y < 0 ? -1 : 1;

    while (height.contains(p.z) && bounds_2.contains({x, y})) {
        glm::vec3 v1 = glm::vec3(x, y, getHeight(x, y));
        glm::vec3 v2 = glm::vec3(x + 1, y, getHeight(x + 1, y));
        glm::vec3 v3 = glm::vec3(x, y + 1, getHeight(x, y + 1));
        glm::vec3 v4 = glm::vec3(x + 1, y + 1, getHeight(x + 1, y + 1));

        d = ::intersect(r, plane(v2, v4, v3));
        if (d.first) {
            glm::vec2 rel = (r.point(d.second) - v1).xy();
            if (quad.contains(rel) && rel.x >= 1 - rel.y) {
                return std::make_pair(true, d.second);
            }
        }

        d = ::intersect(r, plane(v1, v2, v3));
        if (d.first) {
            glm::vec2 rel = (r.point(d.second) - v1).xy();
            if (quad.contains(rel) && rel.x <= 1 - rel.y) {
                return std::make_pair(true, d.second);
            }
        }

        float xDist = almostZero(r.direction.x) ? std::numeric_limits<float>::max() : (x - p.x + flipX) / r.direction.x;
        float yDist = almostZero(r.direction.y) ? std::numeric_limits<float>::max() : (y - p.y + flipY) / r.direction.y;

        if (xDist < yDist) {
            x += dx;
            p += r.direction * xDist;
        } else {
            y += dy;
            p += r.direction * yDist;
        }
    }

    return std::make_pair(false, 0.0f);
}
