// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GEOMETRY__GEOMETRY_H
#define WARSTAGE__GEOMETRY__GEOMETRY_H

#include "./bounds.h"
#include <utility>


inline float angle(glm::vec2 v) { return atan2f(v.y, v.x); }

inline glm::dvec2 vector2_from_angle(double a) { return glm::dvec2(cos(a), sin(a)); }
inline glm::vec2 vector2_from_angle(float a) { return glm::vec2(cosf(a), sinf(a)); }

inline glm::vec2 rotate(glm::vec2 v, float a) { return glm::length(v) * vector2_from_angle(angle(v) + a); }

inline float diff_radians(float a1, float a2) {
    float result = a1 - a2;
    while (result > M_PI)
        result -= 2 * M_PI;
    while (result <= -M_PI)
        result += 2 * M_PI;
    return result;
}

inline float diff_degrees(float a1, float a2) {
    float result = a1 - a2;
    while (result > 180)
        result -= 2 * 180;
    while (result <= -180)
        result += 2 * 180;
    return result;
}


struct plane {
    float a;
    float b;
    float c;
    float d;

    plane() : a(0), b(0), c(0), d(0) {}
    plane(glm::vec3 n, float k) : a(n.x), b(n.y), c(n.z), d(-k) {}
    plane(glm::vec3 n, glm::vec3 p);
    plane(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3);
    plane(const plane& p) = default;

    glm::vec3 normal() const { return glm::vec3(a, b, c); }

    //glm::vec3 project(const glm::vec3& v) const;

};


struct ray {
    glm::vec3 origin;
    glm::vec3 direction;

    ray() : origin(0, 0, 0), direction(0, 0, 1) {}
    ray(glm::vec3 orig, glm::vec3 dir) : origin(orig), direction(dir) {}
    ray(const ray& r) = default;

    glm::vec3 point(float distance) const { return origin + distance * direction; }
};


float distance(glm::vec3 v, plane p);
std::pair<bool, float> intersect(ray r, plane p);
std::pair<bool, float> intersect(ray r, bounds3f b);


#endif
