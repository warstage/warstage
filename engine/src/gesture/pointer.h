// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GESTURE__POINTER_H
#define WARSTAGE__GESTURE__POINTER_H

#include "geometry/velocity-sampler.h"
#include <memory>
#include <vector>


class Gesture;


struct MouseButtons {
    bool left, right, other;
    MouseButtons(bool l = false, bool r = false, bool o = false) : left(l), right(r), other(o) {}
    bool Any() const { return left || right || other; }
};

inline bool operator==(MouseButtons v1, MouseButtons v2) { return v1.left == v2.left && v1.right == v2.right && v1.other == v2.other; }
inline bool operator!=(MouseButtons v1, MouseButtons v2) { return v1.left != v2.left || v1.right != v2.right || v1.other != v2.other; }

enum class PointerType { Mouse, Touch, Stylus };

enum class Motion { Unknown, Stationary, Moving };


class Pointer {
    friend class Gesture;

    std::vector<Gesture*> subscribedGestures_{};
    Gesture* capturedByGesture_{};

    PointerType pointerType_{};
    int tapCount_{};
    bool hasMoved_{};
    glm::vec2 position_{};
    glm::vec2 previous_{};
    glm::vec2 original_{};
    double timestart_{};
    double timestamp_{};
    VelocitySampler sampler_{};
    MouseButtons currentButtons_{};
    MouseButtons previousButtons_{};

public:
    Pointer(PointerType pointerType, int tapCount, glm::vec2 position, double timestamp, MouseButtons buttons);
    ~Pointer();

    Pointer(const Pointer&) = delete;
    Pointer& operator=(const Pointer&) = delete;

    bool IsCaptured() const;
    bool HasSubscribers() const;

    PointerType GetPointerType() const { return pointerType_; }
    int GetTapCount() const { return tapCount_; }

    void TouchBegan();
    void TouchMoved();
    void TouchEnded();
    void TouchCancelled();

    void Update(glm::vec2 position, glm::vec2 previous, double timestamp);
    void Update(glm::vec2 position, double timestamp, MouseButtons buttons);
    void Update(double timestamp);

    glm::vec2 GetCurrentPosition() const;
    glm::vec2 GetPreviousPosition() const;
    glm::vec2 GetOriginalPosition() const;
    void ResetPosition(glm::vec2 position);

    double GetTimestamp() const;

    MouseButtons GetCurrentButtons() const;
    MouseButtons GetPreviousButtons() const;

    Motion GetMotion() const;

    bool HasMoved() const;
    void ResetHasMoved();

    void ResetVelocity();
    glm::vec2 GetVelocity() const;
    glm::vec2 GetVelocity(double timestamp) const;
};


#endif
