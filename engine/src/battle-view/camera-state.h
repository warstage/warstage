// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_VIEW__CAMERA_STATE_H
#define WARSTAGE__BATTLE_VIEW__CAMERA_STATE_H

#include "geometry/bounds.h"
#include "geometry/geometry.h"
#include "battle-model/height-map.h"
#include "graphics/viewport.h"
#include "async/strand.h"
#include <memory>


class HeightMap;

class CameraState {
    bounds2f viewportBounds_{};
    float viewportScaling_{};
    const HeightMap* heightMap_{};
    glm::vec3 cameraPosition_{};
    float cameraTilt_{0.78539f}; // pi / 4
    float cameraFacing_{};
    glm::mat4 transform_{};

public:
    CameraState(bounds2f viewportBounds, float viewportScaling);

    void SetHeightMap(const HeightMap* heightMap, bool moveCamera);

    bounds2i GetViewportBounds() const { return static_cast<bounds2i>(viewportBounds_); }
    void SetViewportBounds(bounds2f viewportBounds, float viewportScaling);

    glm::mat4 GetTransform() const { return transform_; }
    void UpdateTransform();
    glm::mat4 CalculateTransform() const;

    glm::vec3 GetCameraPosition() const { return cameraPosition_; }
    void SetCameraPosition(const glm::vec3& value);
    float GetCameraFacing() const { return cameraFacing_; }
    void SetCameraFacing(float value);
    float GetCameraTilt() const { return cameraTilt_; }
    void SetCameraTilt(float value);

    glm::vec3 GetCameraDirection() const;
    glm::vec3 GetCameraUpVector() const;

    glm::vec2 WindowToNormalized(glm::vec2 value) const;
    glm::vec2 NormalizedToWindow(glm::vec2 value) const;
    glm::vec2 ContentToWindow(glm::vec3 value) const;

    ray GetCameraRay(glm::vec2 screenPosition) const;
    glm::vec3 GetTerrainPosition2(glm::vec2 screenPosition, float height = 0) const;
    glm::vec3 GetTerrainPosition3(glm::vec2 screenPosition) const;

    bounds2f GetUnitMarkerBounds(glm::vec3 position);
    bounds2f GetUnitFacingMarkerBounds(glm::vec2 center, float direction);
    bounds1f GetUnitIconSizeLimit() const;

protected:
    const HeightMap* GetHeightMap() const { return heightMap_; }
};


#endif
