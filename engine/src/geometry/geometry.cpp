// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <glm/gtc/type_ptr.hpp>
#include "./geometry.h"


plane::plane(glm::vec3 n, glm::vec3 p) :
    a(n.x),
    b(n.y),
    c(n.z),
    d(-glm::dot(n, p)) {
}


plane::plane(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3) {
    glm::vec3 n = glm::normalize(glm::cross(v2 - v1, v3 - v1));
    a = n.x;
    b = n.y;
    c = n.z;
    d = -glm::dot(n, v1);
}


/*glm::vec3 plane::project(const glm::vec3& v) const
{
	glm::mat3x3 m;
	glm::vec3::value_type* mm = glm::value_ptr(m);
	mm[0] = 1.0f - a * a;
	mm[1] = -a * b;
	mm[2] = -a * c;
	mm[3] = -b * a;
	mm[4] = 1.0f - b * b;
	mm[5] = -b * c;
	mm[6] = -c * a;
	mm[7] = -c * b;
	mm[8] = 1.0f - c * c;
	return m * v;
}*/


static bool almostZero(float value) {
    static const float epsilon = 10 * std::numeric_limits<float>::epsilon();
    return fabsf(value) < epsilon;
}


float distance(glm::vec3 v, plane p) {
    return glm::dot(p.normal(), v) + p.d;
}


std::pair<bool, float> intersect(ray r, plane p) {
    float result;

    float denom = glm::dot(p.normal(), r.direction);
    if (almostZero(denom))
        return std::make_pair(false, 0.0f);

    float nom = glm::dot(p.normal(), r.origin) + p.d;
    result = -(nom / denom);
    return std::make_pair(true, result);
}


std::pair<bool, float> intersect(ray r, bounds3f b) {
    if (b.empty())
        return std::make_pair(false, 0.0f);

    if (b.contains(r.origin))
        return std::make_pair(true, 0.0f);

    glm::vec3 e = glm::vec3(0.001, 0.001, 0.001);
    bounds3f b2(b.min - e, b.max + e);
    bool hit = false;
    float result = 0;

    if (r.origin.x <= b.min.x && r.direction.x > 0) // min x
    {
        float t = (b.min.x - r.origin.x) / r.direction.x;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.y >= b2.min.y && p.y <= b2.max.y &&
                p.z >= b2.min.z && p.z <= b2.max.z &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    if (r.origin.x >= b.max.x && r.direction.x < 0) // max x
    {
        float t = (b.max.x - r.origin.x) / r.direction.x;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.y >= b2.min.y && p.y <= b2.max.y &&
                p.z >= b2.min.z && p.z <= b2.max.z &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    if (r.origin.y <= b.min.y && r.direction.y > 0) // min y
    {
        float t = (b.min.y - r.origin.y) / r.direction.y;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.x >= b2.min.x && p.x <= b2.max.x &&
                p.z >= b2.min.z && p.z <= b2.max.z &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    if (r.origin.y >= b.max.y && r.direction.y < 0) // max y
    {
        float t = (b.max.y - r.origin.y) / r.direction.y;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.x >= b2.min.x && p.x <= b2.max.x &&
                p.z >= b2.min.z && p.z <= b2.max.z &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    if (r.origin.z <= b.min.z && r.direction.z > 0) // min z
    {
        float t = (b.min.z - r.origin.z) / r.direction.z;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.x >= b2.min.x && p.x <= b2.max.x &&
                p.y >= b2.min.y && p.y <= b2.max.y &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    if (r.origin.z >= b.max.z && r.direction.z < 0) // max z
    {
        float t = (b.max.z - r.origin.z) / r.direction.z;
        if (t >= 0) {
            glm::vec3 p = r.point(t);
            if (p.x >= b2.min.x && p.x <= b2.max.x &&
                p.y >= b2.min.y && p.y <= b2.max.y &&
                (!hit || t < result)) {
                hit = true;
                result = t;
            }
        }
    }

    return std::make_pair(hit, result);
}
