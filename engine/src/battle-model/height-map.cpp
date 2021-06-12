// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./height-map.h"
#include <algorithm>

namespace {

    float nearest_odd(float value) {
        return 1.0f + 2.0f * glm::round(0.5f * (value - 1.0f));
    }

}


HeightMap::HeightMap(bounds2f bounds) :
        bounds_{bounds} {
}


void HeightMap::update(glm::ivec2 dim, const std::function<float(int, int)>& height) {
    dim_ = dim;
    updateHeights_(height);
    updateNormals_();
}


float HeightMap::getHeight(const int x, const int y) const {
    const auto cx = glm::clamp(x, 0, dim_.x - 1);
    const auto cy = glm::clamp(y, 0, dim_.y - 1);
    return heights_[cx + cy * dim_.x];
}


glm::vec3 HeightMap::getNormal(const int x, const int y) const {
    const auto cx = glm::clamp(x, 0, dim_.x - 1);
    const auto cy = glm::clamp(y, 0, dim_.y - 1);
    return normals_[cx + cy * dim_.x];
}


float HeightMap::interpolateHeight(glm::vec2 position) const {
    auto p = (position - bounds_.min) / bounds_.size();

    float x = p.x * static_cast<float>(dim_.x);
    float y = p.y * static_cast<float>(dim_.x);

    // find triangle midpoint coordinates (x1, y1)

    float x1 = nearest_odd(x);
    float y1 = nearest_odd(y);

    // find triangle {(x1, y1), (x2, y2), (x3, y3)} containing (x, y)

    float sx2, sx3, sy2, sy3;
    float dx = x - x1;
    float dy = y - y1;
    if (glm::abs(dx) > glm::abs(dy)) {
        sx2 = sx3 = dx < 0.0f ? -1.0f : 1.0f;
        sy2 = -1;
        sy3 = 1;
    } else {
        sx2 = -1;
        sx3 = 1;
        sy2 = sy3 = dy < 0.0f ? -1.0f : 1.0f;
    }

    float x2 = x1 + sx2;
    float x3 = x1 + sx3;
    float y2 = y1 + sy2;
    float y3 = y1 + sy3;

    // get heights for triangle vertices

    float h1 = getHeight(static_cast<int>(x1), static_cast<int>(y1));
    float h2 = getHeight(static_cast<int>(x2), static_cast<int>(y2));
    float h3 = getHeight(static_cast<int>(x3), static_cast<int>(y3));

    // calculate barycentric coordinates k1, k2, k3
    // note: scale of each k is twice the normal

    float k2 = dx * sx2 + dy * sy2;
    float k3 = dx * sx3 + dy * sy3;
    float k1 = 2.0f - k2 - k3;

    return 0.5f * (k1 * h1 + k2 * h2 + k3 * h3);
}


namespace {
    bool almostZero(float value) {
        static const float epsilon = 10 * std::numeric_limits<float>::epsilon();
        return fabsf(value) < epsilon;
    }
}


std::pair<bool, float> HeightMap::intersect(ray r) const {
    auto offset = glm::vec3{bounds_.min, 0};
    auto scale = glm::vec3{glm::vec2{dim_.x, dim_.y} / bounds_.size(), 1};

    auto r2 = ray{scale * (r.origin - offset), glm::normalize(scale * r.direction)};
    auto d = intersect_(r2);
    if (!d.first)
        return std::make_pair(false, 0.0f);

    float result = glm::length((r2.point(d.second) - r2.origin) / scale);
    return std::make_pair(true, result);
}


std::pair<bool, float> HeightMap::intersect_(ray r) const {
    auto height = bounds1f{-2.5f, 250};
    auto bounds = bounds2f{0.0f, 0.0f, static_cast<float>(dim_.x - 1), static_cast<float>(dim_.y - 1)};
    auto quad = bounds2f{-0.01f, -0.01f, 1.01f, 1.01f};

    auto d = ::intersect(r, bounds3f(bounds, height));
    if (!d.first)
        return std::make_pair(false, 0.0f);

    auto p = r.point(d.second);

    auto bounds_2 = bounds2f{0, 0, static_cast<float>(dim_.x - 2), static_cast<float>(dim_.y - 2)};

    int x = static_cast<int>(bounds_2.x().clamp(p.x));
    int y = static_cast<int>(bounds_2.y().clamp(p.y));
    auto flipX = r.direction.x < 0 ? 0.0f : 1.0f;
    auto flipY = r.direction.y < 0 ? 0.0f : 1.0f;
    int dx = r.direction.x < 0 ? -1 : 1;
    int dy = r.direction.y < 0 ? -1 : 1;

    height = height.add_radius(0.1f); // workaround for intersect precision problem

    while (height.contains(p.z) && bounds_2.contains({x, y})) {
        auto p00 = glm::vec3{x, y, getHeight(x, y)};
        auto p10 = glm::vec3{x + 1, y, getHeight(x + 1, y)};
        auto p01 = glm::vec3{x, y + 1, getHeight(x, y + 1)};
        auto p11 = glm::vec3{x + 1, y + 1, getHeight(x + 1, y + 1)};

        if ((x & 1) == (y & 1)) {
            d = ::intersect(r, plane{p00, p10, p11});
            if (d.first) {
                glm::vec2 rel = (r.point(d.second) - p00).xy();
                if (quad.contains(rel) && rel.x >= rel.y) {
                    return std::make_pair(true, d.second);
                }
            }

            d = ::intersect(r, plane(p00, p11, p01));
            if (d.first) {
                auto rel = (r.point(d.second) - p00).xy();
                if (quad.contains(rel) && rel.x <= rel.y) {
                    return std::make_pair(true, d.second);
                }
            }
        } else {
            d = ::intersect(r, plane(p11, p01, p10));
            if (d.first) {
                auto rel = (r.point(d.second) - p00).xy();
                if (quad.contains(rel) && rel.x >= 1 - rel.y) {
                    return std::make_pair(true, d.second);
                }
            }

            d = ::intersect(r, plane(p00, p10, p01));
            if (d.first) {
                auto rel = (r.point(d.second) - p00).xy();
                if (quad.contains(rel) && rel.x <= 1 - rel.y) {
                    return std::make_pair(true, d.second);
                }
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


void HeightMap::updateHeights_(const std::function<float(int, int)>& height) {
    heights_.resize(dim_.x * dim_.y);

    int nx = dim_.x - 1;
    int ny = dim_.y - 1;

    for (int y = 0; y <= ny; y += 2) {
        for (int x = 0; x <= nx; x += 2) {
            int i = x + y * dim_.x;
            heights_[i] = height(x, y);
        }
    }
    for (int y = 1; y < ny; y += 2) {
        for (int x = 1; x < nx; x += 2) {
            int i = x + y * dim_.x;
            heights_[i] = height(x, y);
        }
    }
    for (int y = 0; y <= ny; y += 2) {
        for (int x = 1; x < nx; x += 2) {
            int i = x + y * dim_.x;
            heights_[i] = 0.5f * (heights_[i - 1] + heights_[i + 1]);
        }
    }
    for (int y = 1; y < ny; y += 2) {
        for (int x = 0; x <= nx; x += 2) {
            int i = x + y * dim_.x;
            heights_[i] = 0.5f * (heights_[i - dim_.x] + heights_[i + dim_.x]);
        }
    }
}


void HeightMap::updateNormals_() {
    normals_.resize(dim_.x * dim_.y);
    int nx = dim_.x - 1;
    int ny = dim_.y - 1;
    auto delta = 2.0f * bounds_.size() / glm::fvec2(nx, ny);
    for (int y = 0; y <= ny; ++y) {
        for (int x = 0; x <= nx; ++x) {
            int index = x + y * dim_.x;
            int index_xn = x != 0 ? index - 1 : index;
            int index_xp = x != nx ? index + 1 : index;
            int index_yn = y != 0 ? index - dim_.x : index;
            int index_yp = y != ny ? index + dim_.x : index;

            float delta_hx = heights_[index_xp] - heights_[index_xn];
            float delta_hy = heights_[index_yp] - heights_[index_yn];

            auto v1 = glm::vec3{delta.x, 0, delta_hx};
            auto v2 = glm::vec3{0, delta.y, delta_hy};

            normals_[index] = glm::normalize(glm::cross(v1, v2));
        }
    }
}
