// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./b-spline.h"
#include "./geometry.h"


const glm::mat4 BSpline::matrix(
    glm::mat4(
        -1, 3, -3, 1,
        3, -6, 3, 0,
        -3, 0, 3, 0,
        1, 4, 1, 0) / 6.0f
);


const glm::mat4 BSpline::matrix_transpose(
    glm::mat4(
        -1, 3, -3, 1,
        3, -6, 0, 4,
        -3, 3, 3, 1,
        1, 0, 0, 0) / 6.0f
);


const glm::mat4 BSpline::split_right(
    glm::mat4(
        0, 1, 6, 1,
        0, 4, 4, 0,
        1, 6, 1, 0,
        4, 4, 0, 0
    ) / 8.0f
);


const glm::mat4 BSpline::split_left(
    glm::mat4(
        0, 0, 4, 4,
        0, 1, 6, 1,
        0, 4, 4, 0,
        1, 6, 1, 0
    ) / 8.0f
);


glm::mat4 BSpline::matrixProduct(const glm::mat4& p) {
    return BSpline::matrix_transpose * p * BSpline::matrix;
}


namespace {
    bool shouldJoin(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float tolerance) {
        if (glm::distance(p0, p2) > 25)
            return false;

        glm::vec2 d1 = p1 - p0;
        glm::vec2 d2 = p2 - p0;
        glm::vec2 x2 = d2 * glm::length(d1) / glm::length(d2);
        return glm::distance(d1, x2) < tolerance;
    }
    bool shouldSplit(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, float tolerance) {
        return !shouldJoin(p0, p1, p2, tolerance) || !shouldJoin(p1, p2, p3, tolerance);
    }
}


void BSpline::join(std::vector<glm::vec2>& path, float tolerance) {
    auto i = path.begin() + 2;
    while (i < path.end()) {
        if (shouldJoin(*(i - 2), *(i - 1), *i, tolerance))
            i = path.erase(i - 1) + 1;
        else
            ++i;
    }
}


void BSpline::split(std::vector<glm::vec2>& path, float tolerance) {
    auto i = path.begin() + 3;
    while (i < path.end()) {
        glm::vec2 p0 = *(i - 3);
        glm::vec2 p1 = *(i - 2);
        glm::vec2 p2 = *(i - 1);
        glm::vec2 p3 = *i;

        if (shouldSplit(p0, p1, p2, p3, tolerance)) {
            glm::vec4 px = glm::vec4(p0.x, p1.x, p2.x, p3.x);
            glm::vec4 py = glm::vec4(p0.y, p1.y, p2.y, p3.y);

            glm::vec4 plx = px * BSpline::split_left;
            glm::vec4 ply = py * BSpline::split_left;
            glm::vec4 prx = px * BSpline::split_right;
            glm::vec4 pry = py * BSpline::split_right;

            i = path.erase(i - 2, i);
            i = path.insert(i, glm::vec2(plx[0], ply[0]));
            i = path.insert(i, glm::vec2(plx[1], ply[1]));
            i = path.insert(i, glm::vec2(plx[2], ply[2]));
            i = path.insert(i, glm::vec2(prx[2], pry[2]));
            i = path.insert(i, glm::vec2(prx[3], pry[3]));
            i += 2;
        } else {
            ++i;
        }
    }
}


std::vector<std::pair<glm::vec2, glm::vec2>> BSpline::lineStrip(const std::vector<glm::vec2>& path) {
    std::vector<std::pair<glm::vec2, glm::vec2>> result;
    if (path.size() >= 4) {
        for (auto i = path.begin() + 2; i < path.end(); ++i) {
            glm::vec2 p = (*(i - 2) + *(i - 1) * 4.0f + *i) / 6.0f;
            glm::vec2 v = 0.5f * (*i - *(i - 2));
            result.emplace_back(p, v);
        }
    }
    return result;
}


std::vector<glm::vec2> BSpline::offset(std::vector<std::pair<glm::vec2, glm::vec2>>& strip, float offset) {
    std::vector<glm::vec2> result{strip.size()};
    auto dst = result.begin();
    for (auto i : strip) {
        *dst++ = {i.first + offset * glm::normalize(glm::vec2(-i.second.y, i.second.x))};
    }

    return result;
}


