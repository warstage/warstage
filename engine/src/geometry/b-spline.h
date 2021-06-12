// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__B_SPLINE_H
#define WARSTAGE__GEOMETRY__B_SPLINE_H

#include <vector>
#include <glm/glm.hpp>


struct BSpline {
    static const glm::mat4 matrix;
    static const glm::mat4 matrix_transpose;
    static const glm::mat4 split_right;
    static const glm::mat4 split_left;

    static glm::mat4 matrixProduct(const glm::mat4& p);

    static inline glm::vec4 basisVector(float t) {
        float t2 = t * t;
        float t3 = t * t2;
        return glm::vec4(t3, t2, t, 1);
    }

    static inline float interpolate(const glm::mat4& p, const glm::vec2& t) {
        return glm::dot(basisVector(t.x), matrixProduct(p) * basisVector(t.y));
    }

    static void join(std::vector<glm::vec2>& path, float tolerance);
    static void split(std::vector<glm::vec2>& path, float tolerance);

    static std::vector<std::pair<glm::vec2, glm::vec2>> lineStrip(const std::vector<glm::vec2>& path);
    static std::vector<glm::vec2> offset(std::vector<std::pair<glm::vec2, glm::vec2>>& strip, float offset);

};

#endif
