// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__CAMERA_GESTURE_H
#define WARSTAGE__BATTLE_GESTURES__CAMERA_GESTURE_H

#include "geometry/velocity-sampler.h"
#include "async/strand.h"
#include "runtime/object.h"
#include "gesture/gesture.h"
#include <chrono>

class UnitController;
class CameraControl;


enum class CameraGestureMode { None, MoveAndOrbit, Zoom };


class CameraGesture : public std::enable_shared_from_this<CameraGesture>, public Gesture {
    UnitController* unitController_{};
    CameraControl* cameraControl_{};

    std::chrono::system_clock::time_point lastTick_{};

    glm::vec3 contentPosition1_{};
    glm::vec3 contentPosition2_{};

    VelocitySampler scrollSampler_{};
    glm::vec2 scrollVelocity_{};
    float scrollFactor_{};

    VelocitySampler orbitSampler_{};
    float previousCameraDirection_{};
    float orbitAccumulator_{};
    float orbitVelocity_{};

    bool keyScrollLeft_{};
    bool keyScrollRight_{};
    bool keyScrollForward_{};
    bool keyScrollBackward_{};
    bool keyOrbitLeft_{};
    bool keyOrbitRight_{};
    float keyOrbitMomentum_{};
    glm::vec2 keyScrollMomentum_{};

    ObjectRef debugWorld_{};
    ObjectRef debugScreen_{};

public:
    CameraGesture(Surface* gestureSurface, UnitController* unitController);

    void Animate() override;
    void _Animate(double secondsSinceLastUpdate);

    void KeyDown(char key) override;
    void KeyUp(char key) override;

    void ScrollWheel(glm::vec2 position, glm::vec2 delta) override;
    void Magnify(glm::vec2 position, float magnification) override;

    bool TryCapturePointer(Pointer* pointer) override;
    void AskReleasePointerToAnotherGesture(Pointer* pointer, Gesture* anotherGesture) override;

    void PointerWillBegin(Pointer* pointer) override;
    void PointerHasBegan(Pointer* pointer) override;
    void PointerWasMoved(Pointer* pointer) override;
    void PointerWasEnded(Pointer* pointer) override;
    void PointerWasReleased(Pointer* pointer) override;

private:
    void UpdateMomentumOrbit(double secondsSinceLastUpdate);
    void UpdateMomentumScroll(double secondsSinceLastUpdate);
    void UpdateKeyScroll(double secondsSinceLastUpdate);
    void UpdateKeyOrbit(double secondsSinceLastUpdate);

    void ResetSamples(double timestamp);
    void UpdateSamples(double timestamp);

    glm::vec2 GetScrollVelocity() const;
    float GetOrbitVelocity() const;
    float CalculateOrbitFactor() const;
    float NormalizeScrollSpeed(float value) const;

    void AdjustToKeepInView(float adjustmentFactor, float secondsSinceLastUpdate);

    void RenderDebugWorld();
    void RenderDebugScreen();

    CameraGestureMode GetCameraGestureMode() const;

    glm::vec3 GetContentPosition1() const;
    glm::vec3 GetContentPosition2() const;
    glm::vec3 GetContentPosition2x() const;
    std::pair<glm::vec3, glm::vec3> GetContentPositions() const;

    glm::vec2 GetCurrentScreenPosition1() const;
    glm::vec2 GetCurrentScreenPosition2() const;
    glm::vec2 GetCurrentScreenPosition2x() const;
    std::pair<glm::vec2, glm::vec2> GetCurrentScreenPositions() const;

    glm::vec3 GetOrbitAnchor() const;
    float GetOrbitFactor() const;
    float GetOrbitAngle() const;
};


#endif
