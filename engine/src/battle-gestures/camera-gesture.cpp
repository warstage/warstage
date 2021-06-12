// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "camera-control.h"
#include "camera-gesture.h"
#include "unit-controller.h"
#include <glm/gtc/constants.hpp>


CameraGesture::CameraGesture(Surface* gestureSurface, UnitController* unitController)
    : Gesture{gestureSurface},
    unitController_{unitController} {
    cameraControl_ = unitController->GetCameraControl();

    //_debugWorld = unitController->_battle_mainThread->Objects("DebugWorld").Create();
    //_debugScreen = unitController->_battle_mainThread->Objects("DebugScreen").Create();
}


void CameraGesture::Animate() {
    LOG_ASSERT(Strand::getMain()->isCurrent());

    auto now = std::chrono::system_clock::now();

    float secondsSinceLastTick = 0.001f * std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick_).count();
    if (secondsSinceLastTick < 0)
        secondsSinceLastTick = 0;
    if (secondsSinceLastTick > 1)
        secondsSinceLastTick = 1;

    _Animate(secondsSinceLastTick);

    lastTick_ = now;
}


void CameraGesture::_Animate(double secondsSinceLastUpdate) {
    if (unitController_->acquireTerrainMap()) {
        glm::vec3 position = cameraControl_->GetCameraPosition();
        float facing = cameraControl_->GetCameraFacing();
        float tilt = cameraControl_->GetCameraTilt();

        if (GetCameraGestureMode() != CameraGestureMode::Zoom)
            UpdateMomentumOrbit(secondsSinceLastUpdate);
        else
            orbitVelocity_ = 0;

        if (GetCameraGestureMode() == CameraGestureMode::None)
            UpdateMomentumScroll(secondsSinceLastUpdate);
        else
            scrollVelocity_ = glm::vec2{};

        //_keyOrbitMomentum = 0;
        //_keyScrollMomentum = glm::vec2{};

        UpdateKeyOrbit(secondsSinceLastUpdate);
        UpdateKeyScroll(secondsSinceLastUpdate);

        if (position != cameraControl_->GetCameraPosition()
            || facing != cameraControl_->GetCameraFacing()
            || tilt != cameraControl_->GetCameraTilt()) {
            unitController_->UpdateCameraObject();
        }
        if (debugWorld_)
            RenderDebugWorld();
        if (debugScreen_)
            RenderDebugScreen();
    }
    unitController_->releaseTerrainMap();
}


void CameraGesture::KeyDown(char key) {
    switch (key) {
        case 'W':
            keyScrollForward_ = true;
            break;
        case 'A':
            keyScrollLeft_ = true;
            break;
        case 'S':
            keyScrollBackward_ = true;
            break;
        case 'D':
            keyScrollRight_ = true;
            break;
        case 'Q':
            keyOrbitRight_ = true;
            break;
        case 'E':
            keyOrbitLeft_ = true;
            break;
        default:
            break;
    }
}


void CameraGesture::KeyUp(char key) {
    switch (key) {
        case 'W':
            keyScrollForward_ = false;
            break;
        case 'A':
            keyScrollLeft_ = false;
            break;
        case 'S':
            keyScrollBackward_ = false;
            break;
        case 'D':
            keyScrollRight_ = false;
            break;
        case 'Q':
            keyOrbitRight_ = false;
            break;
        case 'E':
            keyOrbitLeft_ = false;
            break;
        default:
            break;
    }
}


void CameraGesture::ScrollWheel(glm::vec2 position, glm::vec2 delta) {
    Magnify(position, -delta.y / 10);
}


void CameraGesture::Magnify(glm::vec2 /*position*/, float magnification) {
    if (unitController_->acquireTerrainMap()) {
        bounds2i bounds = cameraControl_->GetViewportBounds();
        glm::vec2 p = (glm::vec2)bounds.mid();
        glm::vec2 d1 = glm::vec2(0.1f * bounds.x().size(), 0);
        glm::vec2 d2 = d1 * glm::exp(magnification);

        auto contentPositions = std::make_pair(
            cameraControl_->GetTerrainPosition3(p - d1),
            cameraControl_->GetTerrainPosition3(p + d1));

        auto screenPositions = std::make_pair(p - d2, p + d2);

        cameraControl_->Zoom(contentPositions, screenPositions);
    }
    unitController_->releaseTerrainMap();
}


void CameraGesture::AskReleasePointerToAnotherGesture(Pointer* pointer, Gesture* /*anotherGesture*/) {
    ReleasePointer(pointer);
}


void CameraGesture::PointerWillBegin(Pointer* pointer) {
    SubscribePointer(pointer);
}


void CameraGesture::PointerHasBegan(Pointer* pointer) {
    if (unitController_->acquireTerrainMap()) {
        bounds2f viewportBounds = (bounds2f)cameraControl_->GetViewportBounds();
        if (viewportBounds.contains(pointer->GetCurrentPosition())) {
            if (TryCapturePointer(pointer)) {
                ResetSamples(pointer->GetTimestamp());
                orbitVelocity_ = 0;
                scrollVelocity_ = glm::vec2{};
                unitController_->UpdateCameraObject();
            }
        }
    }
    unitController_->releaseTerrainMap();
}


void CameraGesture::PointerWasMoved(Pointer* pointer) {
    if (unitController_->acquireTerrainMap()) {
        if (HasCapturedPointer(pointer)) {
            switch (GetCameraGestureMode()) {
                case CameraGestureMode::MoveAndOrbit:
                    cameraControl_->Orbit(
                        GetOrbitAnchor(),
                        CalculateOrbitFactor() * GetOrbitAngle());

                    cameraControl_->Move(
                        GetContentPosition1(),
                        GetCurrentScreenPosition1());
                    break;

                case CameraGestureMode::Zoom:
                    cameraControl_->Zoom(
                        GetContentPositions(),
                        GetCurrentScreenPositions());
                    break;

                default:
                    break;
            }

            AdjustToKeepInView(0.5, 0);
            UpdateSamples(pointer->GetTimestamp());
            unitController_->UpdateCameraObject();
        }
    }
    unitController_->releaseTerrainMap();
}


void CameraGesture::PointerWasEnded(Pointer* pointer) {
    if (unitController_->acquireTerrainMap()) {
        if (HasCapturedPointer(pointer)) {
            scrollVelocity_ = GetScrollVelocity();
            orbitVelocity_ = GetOrbitVelocity();
            unitController_->UpdateCameraObject();
        }
    }
    unitController_->releaseTerrainMap();
}


void CameraGesture::PointerWasReleased(Pointer* pointer) {
    if (pointer == GetCapturedPointers()[0])
        contentPosition1_ = contentPosition2_;
}


bool CameraGesture::TryCapturePointer(Pointer* pointer) {
    if (pointer->IsCaptured()
        || GetCapturedPointers().size() == 2
        || !Gesture::TryCapturePointer(pointer)) {
        return false;
    }

    glm::vec3& contentPosition = GetCapturedPointers().size() == 1
        ? contentPosition1_
        : contentPosition2_;
    contentPosition = cameraControl_->GetTerrainPosition3(pointer->GetCurrentPosition());

    return true;
}


void CameraGesture::UpdateMomentumOrbit(double secondsSinceLastUpdate) {
    glm::vec2 screenPosition = GetCurrentScreenPosition1();
    glm::vec3 contentAnchor = cameraControl_->GetTerrainPosition3(screenPosition);

    cameraControl_->Orbit(contentAnchor, (float)secondsSinceLastUpdate * orbitVelocity_);

    float slowdown = HasCapturedPointers() ? -8.0f : -4.0f;
    orbitVelocity_ = orbitVelocity_ * exp2f(slowdown * (float)secondsSinceLastUpdate);
}


void CameraGesture::UpdateMomentumScroll(double secondsSinceLastUpdate) {
    glm::vec2 screenPosition = cameraControl_->NormalizedToWindow(glm::vec2{});
    glm::vec3 contentPosition = cameraControl_->GetTerrainPosition2(screenPosition);

    contentPosition += (float)secondsSinceLastUpdate * glm::vec3(scrollVelocity_, 0);
    cameraControl_->Move(contentPosition, screenPosition);

    scrollVelocity_ = scrollVelocity_ * glm::exp2(-4.0f * (float)secondsSinceLastUpdate);

    AdjustToKeepInView(0.35f, (float)secondsSinceLastUpdate);
}


void CameraGesture::UpdateKeyScroll(double secondsSinceLastUpdate) {
    float limit = 40;
    if (keyScrollLeft_) keyScrollMomentum_.y += limit;
    if (keyScrollRight_) keyScrollMomentum_.y -= limit;
    if (keyScrollForward_) keyScrollMomentum_.x += limit;
    if (keyScrollBackward_) keyScrollMomentum_.x -= limit;

    glm::vec3 pos = cameraControl_->GetCameraPosition();
    glm::vec3 dir = cameraControl_->GetCameraDirection();
    glm::vec2 delta = (float)secondsSinceLastUpdate * glm::log(2.0f + glm::max(0.0f, pos.z)) * rotate(keyScrollMomentum_, angle(dir.xy()));
    cameraControl_->MoveCamera(pos + glm::vec3(delta, 0));

    keyScrollMomentum_ *= glm::exp2(-25.0f * (float)secondsSinceLastUpdate);
}


void CameraGesture::UpdateKeyOrbit(double secondsSinceLastUpdate) {
    if (keyOrbitLeft_) keyOrbitMomentum_ -= 32 * (float)secondsSinceLastUpdate;
    if (keyOrbitRight_) keyOrbitMomentum_ += 32 * (float)secondsSinceLastUpdate;

    glm::vec2 centerScreen = cameraControl_->NormalizedToWindow(glm::vec2{});
    glm::vec3 contentAnchor = cameraControl_->GetTerrainPosition3(centerScreen);
    cameraControl_->Orbit(contentAnchor, (float)secondsSinceLastUpdate * keyOrbitMomentum_);

    keyOrbitMomentum_ *= exp2f(-25 * (float)secondsSinceLastUpdate);
}


void CameraGesture::ResetSamples(double timestamp) {

    previousCameraDirection_ = angle(cameraControl_->GetCameraDirection().xy());
    orbitAccumulator_ = 0;
    scrollFactor_ = CountCapturedePointers() == 1 ? 1.0f : 0.0f;

    glm::vec2 screenPosition = cameraControl_->NormalizedToWindow(glm::vec2{});
    glm::vec3 contentPosition = cameraControl_->GetTerrainPosition2(screenPosition);

    scrollSampler_.clear();
    scrollSampler_.add(timestamp, contentPosition.xy());
    orbitSampler_.clear();
    orbitSampler_.add(timestamp, glm::vec2(orbitAccumulator_, 0));
}


void CameraGesture::UpdateSamples(double timestamp) {
    float currentCameraDirection = angle(cameraControl_->GetCameraDirection().xy());
    float orbitDelta = diff_radians(currentCameraDirection, previousCameraDirection_);

    previousCameraDirection_ = currentCameraDirection;
    orbitAccumulator_ += orbitDelta;

    if (CountCapturedePointers() == 1) {
        float dt = static_cast<float>(timestamp - scrollSampler_.time());
        float k = std::exp2f(-8.0f * dt);
        scrollFactor_ = (1.0f - k) + k * scrollFactor_;
    } else {
        scrollFactor_ = 0.0f;
    }

    glm::vec2 screenPosition = cameraControl_->NormalizedToWindow(glm::vec2{});
    glm::vec3 contentPosition = cameraControl_->GetTerrainPosition2(screenPosition);

    scrollSampler_.add(timestamp, contentPosition.xy());
    orbitSampler_.add(timestamp, glm::vec2(orbitAccumulator_, 0));
}


glm::vec2 CameraGesture::GetScrollVelocity() const {
    double time = scrollSampler_.time();
    double dt = 0.1;
    glm::vec2 p2 = scrollSampler_.get(time);
    glm::vec2 p1 = scrollSampler_.get(time - dt);
    return scrollFactor_ * (p2 - p1) / (float)dt;
}


float CameraGesture::GetOrbitVelocity() const {
    double time = orbitSampler_.time();
    double dt = 0.1;
    float v2 = orbitSampler_.get(time).x;
    float v1 = orbitSampler_.get(time - dt).x;

    return (float)((v2 - v1) / dt);
}


float CameraGesture::CalculateOrbitFactor() const {
    float orbitSpeed = glm::abs(GetOrbitVelocity());
    float scrollSpeed = NormalizeScrollSpeed(glm::length(GetScrollVelocity()));

    float orbitFactor = bounds1f{0, 1}.clamp(orbitSpeed * 0.8f);
    float scrollFactor = bounds1f{0, 1}.clamp(scrollSpeed * 6.0f);
    float combinedFactor = bounds1f{0, 1}.clamp(1.0f + orbitFactor - scrollFactor);

    return combinedFactor * GetOrbitFactor();
}


float CameraGesture::NormalizeScrollSpeed(float value) const {
    glm::vec2 screenCenter = cameraControl_->NormalizedToWindow(glm::vec2{});
    glm::vec3 contentCenter = cameraControl_->GetTerrainPosition3(screenCenter);
    float angle = cameraControl_->GetCameraFacing() + 0.5f * glm::pi<float>();
    glm::vec3 contentPos = contentCenter + glm::vec3{value * vector2_from_angle(angle), 0};
    glm::vec2 screenPos = cameraControl_->ContentToWindow(contentPos);

    glm::ivec2 viewportSize = cameraControl_->GetViewportBounds().size();
    float viewportScale = glm::max(viewportSize.x, viewportSize.y);

    return glm::distance(screenCenter, screenPos) / viewportScale;
}


void CameraGesture::AdjustToKeepInView(float /*adjustmentFactor*/, float secondsSinceLastUpdate) {
    cameraControl_->ClampCameraPosition();

#if false

    bool is_scrolling = glm::length(_scrollVelocity) > 16;
    bool brake_scrolling = false;

    bounds2f viewportBounds = terrainView->GetViewport()->GetBounds();
    glm::vec2 left = terrainView->GetScreenLeft();
    glm::vec2 right = terrainView->GetScreenRight();
    float dx1 = left.x - viewportBounds.min.x;
    float dx2 = viewportBounds.max.x - right.x;

    if (dx1 > 0 || dx2 > 0)
    {
        if (is_scrolling)
        {
            brake_scrolling = true;
        }
        else if (dx1 > 0 && dx2 > 0)
        {
            glm::vec3 contentPosition1 = terrainView->GetTerrainPosition2(left);
            glm::vec3 contentPosition2 = terrainView->GetTerrainPosition2(right);
            left.x -= adjustmentFactor * dx1;
            right.x += adjustmentFactor * dx2;
            terrainView->Zoom(contentPosition1, contentPosition2, left, right);
        }
        else if (dx1 > 0)
        {
            glm::vec3 contentPosition = terrainView->GetTerrainPosition2(left);
            left.x -= adjustmentFactor * dx1;
            terrainView->Move(contentPosition, left);
        }
        else if (dx2 > 0)
        {
            glm::vec3 contentPosition = terrainView->GetTerrainPosition2(right);
            right.x += adjustmentFactor * dx2;
            terrainView->Move(contentPosition, right);
        }
    }

    glm::vec2 bottom = terrainView->GetScreenBottom();
    glm::vec2 top = terrainView->GetScreenTop();
    float dy1 = bottom.y - viewportBounds.min.y;
    float dy2 = viewportBounds.min.y + 0.85f * viewportBounds.max.y - top.y;

    if (dy1 > 0 || dy2 > 0)
    {
        if (is_scrolling)
        {
            brake_scrolling = true;
        }
        else if (dy1 > 0 && dy2 > 0)
        {
            glm::vec3 contentPosition1 = terrainView->GetTerrainPosition2(bottom);
            glm::vec3 contentPosition2 = terrainView->GetTerrainPosition2(top);
            bottom.y -= adjustmentFactor * dy1;
            top.y += adjustmentFactor * dy2;
            terrainView->Zoom(contentPosition1, contentPosition2, bottom, top);
        }
        else if (dy1 > 0)
        {
            glm::vec3 contentPosition = terrainView->GetTerrainPosition2(bottom);
            bottom.y -= adjustmentFactor * dy1;
            terrainView->Move(contentPosition, bottom);
        }
        else if (dy2 > 0)
        {
            glm::vec3 contentPosition = terrainView->GetTerrainPosition2(top);
            top.y += adjustmentFactor * dy2;
            terrainView->Move(contentPosition, top);
        }
    }

    if (brake_scrolling)
    {
        _scrollVelocity = _scrollVelocity * exp2f(-16 * secondsSinceLastUpdate);
    }

#endif
}


static void RenderCross(std::vector<glm::vec3>& vertices, glm::vec3 p, float d) {
    vertices.push_back(p + glm::vec3(0, 0, -d));
    vertices.push_back(p + glm::vec3(0, 0, d));

    vertices.push_back(p + glm::vec3(-d, 0, -d));
    vertices.push_back(p + glm::vec3(d, 0, d));

    vertices.push_back(p + glm::vec3(d, 0, -d));
    vertices.push_back(p + glm::vec3(-d, 0, d));

    vertices.push_back(p + glm::vec3(0, d, -d));
    vertices.push_back(p + glm::vec3(0, -d, d));

    vertices.push_back(p + glm::vec3(0, -d, -d));
    vertices.push_back(p + glm::vec3(0, d, d));
}


static void RenderCircle(std::vector<glm::vec3>& vertices, glm::vec3 p, float d) {
    const float n = 32;
    const float two_pi_over_n = 2 * glm::pi<float>() / n;
    for (float i = 0; i < n; ++i) {
        float a1 = two_pi_over_n * i;
        float a2 = two_pi_over_n * (i + 1);

        glm::vec2 d1 = glm::vec2{glm::cos(a1), glm::sin(a1)};
        glm::vec2 d2 = glm::vec2{glm::cos(a2), glm::sin(a2)};

        vertices.push_back(p + d * glm::vec3{d1, 0});
        vertices.push_back(p + d * glm::vec3{d2, 0});
    }
}


void CameraGesture::RenderDebugWorld() {
    std::vector<glm::vec3> vertices;

    glm::vec3 p1 = GetContentPosition1();
    glm::vec3 p2 = GetContentPosition2();
    RenderCross(vertices, p1, 16);
    RenderCross(vertices, p2, 16);
    vertices.push_back({p1});
    vertices.push_back({p2});

    glm::vec3 c1 = cameraControl_->GetTerrainPosition3(GetCurrentScreenPosition1());
    glm::vec3 c2 = cameraControl_->GetTerrainPosition3(GetCurrentScreenPosition2());
    RenderCross(vertices, c1, 8);
    RenderCross(vertices, c2, 8);
    vertices.push_back({c1});
    vertices.push_back({c2});

    glm::vec3 a = GetOrbitAnchor();
    float d = glm::distance(p1, a);
    RenderCircle(vertices, a, d);

    auto array = Struct{} << "_" << Array();
    for (const auto& v : vertices)
        array = std::move(array) << v;

    debugWorld_["vertices"] = *(std::move(array) << ValueEnd{} << ValueEnd{}).begin();
}


void CameraGesture::RenderDebugScreen() {
    std::vector<glm::vec2> vertices;

    auto bounds = cameraControl_->GetCameraState().GetViewportBounds();
    for (int x = bounds.min.x; x < bounds.max.x; x += 50)
        for (int y = bounds.min.y; y < bounds.max.y; y += 50) {
            auto p = glm::vec2{x, y};
            //auto q = _cameraControl->GetTerrainPosition3(p);
            float angle = 0; //_terrainGesture->GetOrbitAngle(p, q.xy());
            float factor = 0; //_terrainGesture->GetOrbitFactor(p);
            auto r = glm::vec2{glm::cos(angle), glm::sin(angle)} * (2.0f + 20.0f * factor);
            vertices.push_back(p + r);
            vertices.push_back(p - r);
        }


    auto array = Struct{} << "_" << Array();
    for (const auto& v : vertices)
        array = std::move(array) << v;

    debugScreen_["vertices"] = *(std::move(array) << ValueEnd{} << ValueEnd{}).begin();
}


CameraGestureMode CameraGesture::GetCameraGestureMode() const {
    switch (GetCapturedPointers().size()) {
        case 1:
            return CameraGestureMode::MoveAndOrbit;
        case 2:
            return CameraGestureMode::Zoom;
        default:
            return CameraGestureMode::None;
    }
}


glm::vec3 CameraGesture::GetContentPosition1() const {
    if (!GetCapturedPointers().empty())
        return contentPosition1_;

    return cameraControl_->GetTerrainPosition3(GetCurrentScreenPosition1());
}


glm::vec3 CameraGesture::GetContentPosition2() const {
    if (GetCapturedPointers().size() >= 2)
        return contentPosition2_;
    return GetContentPosition2x();
}


glm::vec3 CameraGesture::GetContentPosition2x() const {
    glm::vec3 p = GetContentPosition1();
    glm::vec3 c = cameraControl_->GetTerrainPosition3(cameraControl_->NormalizedToWindow(glm::vec2{}));
    return glm::vec3{2.0f * c.xy() - p.xy(), p.z};
}


std::pair<glm::vec3, glm::vec3> CameraGesture::GetContentPositions() const {
    return std::make_pair(GetContentPosition1(), GetContentPosition2());
}


glm::vec2 CameraGesture::GetCurrentScreenPosition1() const {
    if (!GetCapturedPointers().empty())
        return GetCapturedPointers().front()->GetCurrentPosition();

    return cameraControl_->NormalizedToWindow(glm::vec2{});
}


glm::vec2 CameraGesture::GetCurrentScreenPosition2() const {
    if (GetCapturedPointers().size() >= 2)
        return GetCapturedPointers().back()->GetCurrentPosition();
    return GetCurrentScreenPosition2x();
}


glm::vec2 CameraGesture::GetCurrentScreenPosition2x() const {
    glm::vec3 p = cameraControl_->GetTerrainPosition3(GetCurrentScreenPosition1());
    glm::vec3 c = cameraControl_->GetTerrainPosition3(cameraControl_->NormalizedToWindow(glm::vec2{}));
    return cameraControl_->ContentToWindow(glm::vec3(2.0f * c.xy() - p.xy(), p.z));
}


std::pair<glm::vec2, glm::vec2> CameraGesture::GetCurrentScreenPositions() const {
    return std::make_pair(GetCurrentScreenPosition1(), GetCurrentScreenPosition2());
}


glm::vec3 CameraGesture::GetOrbitAnchor() const {
    return 0.5f * (GetContentPosition1() + GetContentPosition2x());
}


float CameraGesture::GetOrbitFactor() const {
    glm::vec2 pos = GetCapturedPointers().size() == 1
        ? GetCurrentScreenPosition1()
        : 0.5f * (GetCurrentScreenPosition1() + GetCurrentScreenPosition2());

    float value = glm::length(cameraControl_->WindowToNormalized(pos));
    return bounds1f{0, 1}.clamp(bounds1f{0.33f, 0.66f}.unmix(value * value));
}


float CameraGesture::GetOrbitAngle() const {
    glm::vec3 currentPosition1 = cameraControl_->GetTerrainPosition3(GetCurrentScreenPosition1());
    glm::vec3 currentPosition2 = cameraControl_->GetTerrainPosition2(GetCurrentScreenPosition2x(), currentPosition1.z);

    float currentAngle = angle(currentPosition1.xy() - currentPosition2.xy());
    float contentAngle = angle(GetContentPosition1().xy() - GetContentPosition2x().xy());

    return contentAngle - currentAngle;
}
