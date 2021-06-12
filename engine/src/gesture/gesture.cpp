// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./gesture.h"
#include "./surface.h"
#include "./pointer.h"
#include <algorithm>


bool Gesture::HasCapturedPointer(Pointer* pointer) const {
    return std::find(pointersCaptured_.begin(), pointersCaptured_.end(), pointer) != pointersCaptured_.end();
}


bool Gesture::HasCapturedPointer() const {
    return pointersCaptured_.size() == 1;
}


bool Gesture::HasCapturedPointers() const {
    return !pointersCaptured_.empty();
}


Pointer* Gesture::GetCapturedPointer() const {
    return pointersCaptured_.size() == 1 ? pointersCaptured_.front() : nullptr;
}


int Gesture::CountCapturedePointers() const {
    return static_cast<int>(pointersCaptured_.size());
}


const std::vector<Pointer*>& Gesture::GetSubscribedPointers() const {
    return pointersSubscribed_;
}


const std::vector<Pointer*>& Gesture::GetCapturedPointers() const {
    return pointersCaptured_;
}


void Gesture::SubscribePointer(Pointer* pointer) {
    if (std::find(pointersSubscribed_.begin(), pointersSubscribed_.end(), pointer) != pointersSubscribed_.end())
        return;

    pointersSubscribed_.push_back(pointer);
    pointer->subscribedGestures_.push_back(this);
}


void Gesture::UnsubscribePointer(Pointer* pointer) {
    if (HasCapturedPointer(pointer))
        ReleasePointer(pointer);

    pointersSubscribed_.erase(
        std::remove(pointersSubscribed_.begin(), pointersSubscribed_.end(), pointer),
        pointersSubscribed_.end());

    pointer->subscribedGestures_.erase(
        std::remove(pointer->subscribedGestures_.begin(), pointer->subscribedGestures_.end(), this),
        pointer->subscribedGestures_.end());
}


bool Gesture::TryCapturePointer(Pointer* pointer) {
    if (pointer->capturedByGesture_ == this)
        return true;

    if (pointer->capturedByGesture_) {
        auto gesture = pointer->capturedByGesture_;
        gesture->AskReleasePointerToAnotherGesture(pointer, this);
        if (pointer->capturedByGesture_)
            return false;
    }

    pointersCaptured_.push_back(pointer);
    pointer->capturedByGesture_ = this;

    return true;
}


void Gesture::ReleasePointer(Pointer* pointer) {
    if (!HasCapturedPointer(pointer))
        return;

    PointerWasReleased(pointer);

    pointersCaptured_.erase(
        std::remove(pointersCaptured_.begin(), pointersCaptured_.end(), pointer),
        pointersCaptured_.end());

    pointer->capturedByGesture_ = nullptr;
}


Gesture::Gesture(Surface* gestureSurface) :
    gestureSurface_{gestureSurface}
{
    gestureSurface_->gestures_.insert(gestureSurface_->gestures_.begin(), this);
}


Gesture::~Gesture() {
    for (Pointer* pointer : pointersSubscribed_) {
        pointer->subscribedGestures_.erase(
            std::remove(pointer->subscribedGestures_.begin(), pointer->subscribedGestures_.end(), this),
            pointer->subscribedGestures_.end());
    }

    while (!pointersCaptured_.empty())
        ReleasePointer(pointersCaptured_.back());

    while (!pointersSubscribed_.empty())
        UnsubscribePointer(pointersSubscribed_.back());

    gestureSurface_->gestures_.erase(
        std::remove(gestureSurface_->gestures_.begin(), gestureSurface_->gestures_.end(), this),
        gestureSurface_->gestures_.end());
}
