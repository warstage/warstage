// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__CAMERA_CONTROL_H
#define WARSTAGE__BATTLE_GESTURES__CAMERA_CONTROL_H

#include "battle-view/camera-state.h"


class CameraControl : public CameraState {
public:
    CameraControl(bounds2f viewportBounds, float viewportScaling);

    CameraState& GetCameraState() { return *this; }

    void Move(glm::vec3 originalContentPosition, glm::vec2 currentScreenPosition);
    void Zoom(std::pair<glm::vec3, glm::vec3> originalContentPositions, std::pair<glm::vec2, glm::vec2> currentScreenPositions);
    void Orbit(glm::vec3 anchor, float angle);

    void MoveCamera(glm::vec3 position);
    void ClampCameraPosition();
    [[nodiscard]] float ClampCameraHeight(float height) const;
    [[nodiscard]] float CalculateCameraTilt(float height) const;

    void InitializeCameraPosition(int position);
};


#endif
