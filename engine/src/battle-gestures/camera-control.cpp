// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "./camera-control.h"
#include "battle-model/height-map.h"
#include "battle-view/camera-state.h"


CameraControl::CameraControl(bounds2f viewportBounds, float viewportScaling) : CameraState{viewportBounds, viewportScaling} {
}


void CameraControl::Move(glm::vec3 originalContentPosition, glm::vec2 currentScreenPosition) {
    ray ray1 = GetCameraRay(currentScreenPosition);
    ray ray2 = ray(originalContentPosition, -ray1.direction);

    plane cameraPlane(glm::vec3(0, 0, 1), GetCameraPosition());
    auto d = intersect(ray2, cameraPlane);
    if (d.first) {
        MoveCamera(ray2.point(d.second));
    }
}


void CameraControl::Zoom(std::pair<glm::vec3, glm::vec3> originalContentPositions, std::pair<glm::vec2, glm::vec2> currentScreenPositions) {
    glm::vec3 contentAnchor = (originalContentPositions.first + originalContentPositions.second) / 2.0f;
    glm::vec2 currentScreenCenter = (currentScreenPositions.first + currentScreenPositions.second) / 2.0f;
    glm::vec3 originalDelta = originalContentPositions.second - originalContentPositions.first;
    float originalAngle = angle(originalDelta.xy());

    float delta = glm::length(GetHeightMap()->getBounds().size()) / 20.0f;
    for (int i = 0; i < 18; ++i) {
        glm::vec3 currentContentPosition1 = GetTerrainPosition3(currentScreenPositions.first);
        glm::vec3 currentContentPosition2 = GetTerrainPosition3(currentScreenPositions.second);
        glm::vec3 currentDelta = currentContentPosition2 - currentContentPosition1;

        float currentAngle = angle(currentDelta.xy());
        if (glm::abs(diff_radians(originalAngle, currentAngle)) < glm::pi<float>() / 2)
            Orbit(contentAnchor, originalAngle - currentAngle);

        float k = glm::dot(originalDelta, originalDelta) < glm::dot(currentDelta, currentDelta) ? delta : -delta;
        MoveCamera(GetCameraPosition() + k * GetCameraDirection());
        delta *= 0.75;

        Move(contentAnchor, currentScreenCenter);
    }
}


void CameraControl::Orbit(glm::vec3 anchor, float angle) {
    glm::quat rotation = glm::angleAxis(angle, glm::vec3(0, 0, 1));
    glm::vec3 pos = GetCameraPosition();
    glm::vec3 value = anchor + rotation * (pos - anchor);

    SetCameraPosition(value);
    SetCameraFacing(GetCameraFacing() + angle);
}


void CameraControl::MoveCamera(glm::vec3 position) {
    position.z = ClampCameraHeight(position.z);
    float tilt = CalculateCameraTilt(position.z);

    SetCameraPosition(position);
    SetCameraTilt(tilt);
}


void CameraControl::ClampCameraPosition() {
    glm::vec2 centerScreen = (glm::vec2)NormalizedToWindow(glm::vec2(0, 0));
    glm::vec2 contentCamera = GetTerrainPosition2(centerScreen).xy();
    glm::vec2 contentCenter = GetHeightMap()->getBounds().mid();
    float contentRadius = 0.5f * GetHeightMap()->getBounds().x().size();

    glm::vec2 offset = contentCamera - contentCenter;
    float distance = glm::length(offset);
    if (distance > contentRadius) {
        glm::vec2 direction = offset / distance;
        SetCameraPosition(GetCameraPosition() - glm::vec3(direction * (distance - contentRadius), 0));
    }
}


const float mininum_height = 75;


float CameraControl::ClampCameraHeight(float height) const {
    bounds2f bounds = GetHeightMap()->getBounds();
    float radius = 0.5f * glm::length(bounds.size());
    return bounds1f{mininum_height, 1.0f * radius}.clamp(height);
}


float CameraControl::CalculateCameraTilt(float height) const {
    bounds2f bounds = GetHeightMap()->getBounds();
    float radius = 0.5f * glm::length(bounds.size());

    float t = bounds1f{0, 1}.clamp((height - mininum_height) / (radius - mininum_height));

    return bounds1f{0.17f * glm::pi<float>(), 0.40f * glm::pi<float>()}.mix(t);
}


void CameraControl::InitializeCameraPosition(int position) {
    glm::vec2 friendlyCenter;
    glm::vec2 enemyCenter;

    switch (position) {
        case 1:
            friendlyCenter = glm::vec2(512, 512 - 400);
            enemyCenter = glm::vec2(512, 512 + 64);
            break;

        case 2:
            friendlyCenter = glm::vec2(512, 512 + 400);
            enemyCenter = glm::vec2(512, 512 - 64);
            break;
        default:
            friendlyCenter = glm::vec2(512 - 400, 512);
            enemyCenter = glm::vec2(512 + 64, 512);
            break;
    }

    glm::vec2 friendlyScreen = NormalizedToWindow(glm::vec2(0, -0.4));
    glm::vec2 enemyScreen = NormalizedToWindow(glm::vec2(0, 0.4));

    auto contentPositions = std::make_pair(
            GetHeightMap()->getPosition(friendlyCenter, 0),
            GetHeightMap()->getPosition(enemyCenter, 0));
    auto screenPositions = std::make_pair(friendlyScreen, enemyScreen);

    Zoom(contentPositions, screenPositions);
}
