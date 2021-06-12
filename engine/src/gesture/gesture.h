// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GESTURE__GESTURE_H
#define WARSTAGE__GESTURE__GESTURE_H

#include <vector>
#include <glm/glm.hpp>


class Surface;
class Pointer;
class Viewport;


class Gesture {
    Surface* gestureSurface_;
    std::vector<Pointer*> pointersSubscribed_{};
    std::vector<Pointer*> pointersCaptured_{};

public:
    Gesture(Surface* gestureSurface);
    virtual ~Gesture();

    Gesture(const Gesture&) = delete;
    Gesture& operator=(const Gesture&) = delete;

    Surface& GetGestureSurface() const { return *gestureSurface_; }

    bool HasCapturedPointer(Pointer* pointer) const;
    bool HasCapturedPointer() const;
    bool HasCapturedPointers() const;

    Pointer* GetCapturedPointer() const;
    int CountCapturedePointers() const;

    const std::vector<Pointer*>& GetSubscribedPointers() const;
    const std::vector<Pointer*>& GetCapturedPointers() const;

    void SubscribePointer(Pointer* pointer);
    void UnsubscribePointer(Pointer* pointer);

    virtual bool TryCapturePointer(Pointer* pointer);
    virtual void ReleasePointer(Pointer* pointer);

    virtual void Animate() {}

    virtual void KeyDown(char key) {}
    virtual void KeyUp(char key) {}

    virtual void ScrollWheel(glm::vec2 position, glm::vec2 delta) {}
    virtual void Magnify(glm::vec2 position, float magnification) {}

    virtual void AskReleasePointerToAnotherGesture(Pointer* pointer, Gesture* anotherGesture) {}

    virtual void PointerWillBegin(Pointer* pointer) {}
    virtual void PointerHasBegan(Pointer* pointer) {}
    virtual void PointerWasMoved(Pointer* pointer) {}
    virtual void PointerWasEnded(Pointer* pointer) {}
    virtual void PointerWasCancelled(Pointer* pointer) {}

    virtual void PointerWasReleased(Pointer* pointer) {}
};


#endif
