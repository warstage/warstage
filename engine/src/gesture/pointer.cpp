// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./pointer.h"
#include "./gesture.h"


Pointer::Pointer(PointerType pointerType, int tapCount, glm::vec2 position, double timestamp, MouseButtons buttons) :
    capturedByGesture_{nullptr},
    pointerType_{pointerType},
    tapCount_{tapCount},
    hasMoved_{false},
    position_{position},
    previous_{position},
    original_{position},
    timestart_{timestamp},
    timestamp_{timestamp},
    currentButtons_{buttons},
    previousButtons_{}
{
    sampler_.add(timestamp, position);
}


Pointer::~Pointer() {
    if (capturedByGesture_)
        capturedByGesture_->ReleasePointer(this);

    while (!subscribedGestures_.empty()) {
        auto gesture = subscribedGestures_.back();
        gesture->UnsubscribePointer(this);
    }
}


bool Pointer::IsCaptured() const {
    return capturedByGesture_ != nullptr;
}


bool Pointer::HasSubscribers() const {
    return !subscribedGestures_.empty();
}


void Pointer::TouchBegan() {
    std::vector<Gesture*> gestures(subscribedGestures_);
    for (auto gesture : gestures)
        gesture->PointerHasBegan(this);
}


void Pointer::TouchMoved() {
    std::vector<Gesture*> gestures(subscribedGestures_);
    for (auto gesture : gestures)
        gesture->PointerWasMoved(this);
}


void Pointer::TouchEnded() {
    std::vector<Gesture*> gestures(subscribedGestures_);
    for (auto gesture : gestures)
        gesture->PointerWasEnded(this);
}


void Pointer::TouchCancelled() {
    std::vector<Gesture*> gestures(subscribedGestures_);
    for (auto gesture : gestures)
        gesture->PointerWasCancelled(this);
}


void Pointer::Update(glm::vec2 position, glm::vec2 previous, double timestamp) {
    timestamp_ = timestamp;
    previous_ = previous;
    position_ = position;

    sampler_.add(timestamp, position);

    if (GetMotion() == Motion::Moving)
        hasMoved_ = true;
}


void Pointer::Update(glm::vec2 position, double timestamp, MouseButtons buttons) {
    timestamp_ = timestamp;
    previous_ = position_;
    position_ = position;

    previousButtons_ = currentButtons_;
    currentButtons_ = buttons;

    sampler_.add(timestamp, position);

    if (GetMotion() == Motion::Moving)
        hasMoved_ = true;
}


void Pointer::Update(double timestamp) {
    if (timestamp - timestamp_ > 0.15) {
        previous_ = position_;
        timestamp_ = timestamp;
    }
}


glm::vec2 Pointer::GetCurrentPosition() const {
    return position_;
}


glm::vec2 Pointer::GetPreviousPosition() const {
    return previous_;
}


glm::vec2 Pointer::GetOriginalPosition() const {
    return original_;
}


void Pointer::ResetPosition(glm::vec2 position) {
    position_ = position;
    previous_ = position;
    original_ = position;
}


double Pointer::GetTimestamp() const {
    return timestamp_;
}


MouseButtons Pointer::GetCurrentButtons() const {
    return currentButtons_;
}


MouseButtons Pointer::GetPreviousButtons() const {
    return previousButtons_;
}


Motion Pointer::GetMotion() const {
    if (timestamp_ - sampler_.time() > 0.15)
        return Motion::Stationary;

    float speed = glm::length(GetVelocity(timestamp_));

    if (speed > 50)
        return Motion::Moving;

    if (timestamp_ - timestart_ < 0.2)
        return Motion::Unknown;

    if (speed < 1)
        return Motion::Stationary;

    return Motion::Unknown;
}


bool Pointer::HasMoved() const {
    return hasMoved_;
}


void Pointer::ResetHasMoved() {
    hasMoved_ = false;
    original_ = position_;
}


void Pointer::ResetVelocity() {
    sampler_.clear();
    sampler_.add(timestamp_, position_);
}


glm::vec2 Pointer::GetVelocity() const {
    return GetVelocity(timestamp_);
}


glm::vec2 Pointer::GetVelocity(double timestamp) const {
    float dt = 0.1f;
    glm::vec2 p1 = sampler_.get(timestamp - dt);
    glm::vec2 p2 = sampler_.get(timestamp);

    return (p2 - p1) / dt;
}
