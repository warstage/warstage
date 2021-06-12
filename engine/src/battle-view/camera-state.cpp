// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./camera-state.h"
#include "geometry/geometry.h"
#include "battle-model/terrain-map.h"
#include "battle-model/height-map.h"
#include "graphics/graphics.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>


CameraState::CameraState(bounds2f viewportBounds, float viewportScaling) :
    viewportBounds_{viewportBounds},
    viewportScaling_{viewportScaling},
    heightMap_{&TerrainMap::getBlankMap()->getHeightMap()} {
}


void CameraState::SetHeightMap(const HeightMap* heightMap, bool moveCamera) {
    heightMap_ = heightMap;
    if (moveCamera) {
        bounds2f bounds = heightMap->getBounds();
        float height = 0.3f * glm::length(bounds.size());
        SetCameraPosition(glm::vec3(bounds.mid(), height));
    }
}

void CameraState::SetViewportBounds(bounds2f viewportBounds, float viewportScaling) {
    viewportBounds_ = viewportBounds;
    viewportScaling_ = viewportScaling;
}


void CameraState::UpdateTransform() {
    transform_ = CalculateTransform();
}


glm::mat4 CameraState::CalculateTransform() const {
    float r = 2.0f * glm::length(heightMap_->getBounds().size());
    glm::vec2 viewportSize = (glm::vec2)viewportBounds_.size();
    float aspect = viewportSize.y / viewportSize.x;
    glm::mat4 proj = glm::perspective(45.0f, 1.0f / aspect, 0.01f * r, r);
    glm::mat4 view = glm::lookAt(cameraPosition_, cameraPosition_ + GetCameraDirection(), GetCameraUpVector());
    return proj * view;
}


void CameraState::SetCameraPosition(const glm::vec3& value) {
    cameraPosition_ = value;
}


void CameraState::SetCameraFacing(float value) {
    cameraFacing_ = value;
}


void CameraState::SetCameraTilt(float value) {
    cameraTilt_ = value;
}


glm::vec3 CameraState::GetCameraDirection() const {
    glm::quat q = glm::angleAxis(cameraFacing_, glm::vec3(0, 0, 1));
    glm::vec3 v(cosf(cameraTilt_), 0, -sinf(cameraTilt_));
    return q * v;
}


glm::vec3 CameraState::GetCameraUpVector() const {
    glm::quat q = glm::angleAxis(cameraFacing_, glm::vec3(0, 0, 1));
    glm::vec3 v(sinf(cameraTilt_), 0, cosf(cameraTilt_));
    return q * v;
}


glm::vec2 CameraState::WindowToNormalized(glm::vec2 value) const {
    glm::ivec2 size = viewportBounds_.size();
    if (size.x == 0 || size.y == 0)
        return glm::vec2(-1, -1);

    return 2.0f * value / (glm::vec2)size - 1.0f;
}


glm::vec2 CameraState::NormalizedToWindow(glm::vec2 value) const {
    glm::ivec2 size = viewportBounds_.size();
    if (size.x == 0 || size.y == 0)
        return glm::vec2(0, 0);

    return (value + 1.0f) / 2.0f * (glm::vec2)size;
}


glm::vec2 CameraState::ContentToWindow(glm::vec3 value) const {
    glm::mat4 transform = CalculateTransform();
    glm::vec4 p = transform * glm::vec4{value, 1};
    return NormalizedToWindow({p.x / p.w, p.y / p.w});
}


ray CameraState::GetCameraRay(glm::vec2 screenPosition) const {
    glm::vec2 viewPosition = WindowToNormalized(screenPosition);
    glm::mat4 inverse = glm::inverse(CalculateTransform());
    glm::vec4 p1 = inverse * glm::vec4(viewPosition, 0, 1.0f);
    glm::vec4 p2 = inverse * glm::vec4(viewPosition, 0.5f, 1.0f);

    glm::vec3 q1 = (glm::vec3)p1.xyz() / p1.w;
    glm::vec3 q2 = (glm::vec3)p2.xyz() / p2.w;

    glm::vec3 direction = glm::normalize(q2 - q1);
    glm::vec3 origin = q1 - 200.0f * direction;
    return ray(origin, direction);
}


glm::vec3 CameraState::GetTerrainPosition2(glm::vec2 screenPosition, float height) const {
    ray r = GetCameraRay(screenPosition);
    std::pair<bool, float> d = intersect(r, plane(glm::vec3(0, 0, 1), height));
    return r.point(d.first ? d.second : 0);
}


glm::vec3 CameraState::GetTerrainPosition3(glm::vec2 screenPosition) const {
    ray r = GetCameraRay(screenPosition);
    std::pair<bool, float> d = heightMap_->intersect(r);

    if (d.first) {
        glm::vec3 p = r.point(d.first ? d.second : 0);
        if (p.z > 0 && glm::distance(heightMap_->getBounds().unmix(p.xy()), glm::vec2{0.5f}) < 0.5f)
            return p;
    }

    return GetTerrainPosition2(screenPosition);
}


bounds2f CameraState::GetUnitMarkerBounds(glm::vec3 position) {
    glm::mat4 transform = CalculateTransform();
    glm::vec3 upvector = GetCameraUpVector();
    float viewport_height = viewportBounds_.y().size();
    bounds1f sizeLimit = GetUnitIconSizeLimit();

    glm::vec3 position2 = position + 32 * 0.5f * viewport_height * upvector;
    glm::vec4 p = transform * glm::vec4(position, 1);
    glm::vec4 q = transform * glm::vec4(position2, 1);
    float s = glm::clamp(glm::abs(q.y / q.w - p.y / p.w), sizeLimit.min, sizeLimit.max);

    return bounds2f(NormalizedToWindow((glm::vec2)p.xy() / p.w)).add_radius(s / 2);
}


bounds2f CameraState::GetUnitFacingMarkerBounds(glm::vec2 center, float direction) {
    bounds2f iconBounds = GetUnitMarkerBounds(heightMap_->getPosition(center, 0));

    glm::vec2 position = iconBounds.mid();
    float size = iconBounds.y().size();
    float adjust = glm::half_pi<float>();

    position += 0.7f * size * vector2_from_angle(direction - GetCameraFacing() + adjust);

    return bounds2f(position).add_radius(0.2f * size);
}


bounds1f CameraState::GetUnitIconSizeLimit() const {
    float y = GetCameraDirection().z;
    float x = sqrtf(1 - y * y);
    float a = 1 - fabsf(atan2f(y, x) / (float)M_PI_2);

    bounds1f result(0, 0);
    result.min = 32 - 8 * a;
    result.max = 32 + 16 * a;
    return viewportScaling_ * result;
}
