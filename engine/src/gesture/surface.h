// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GESTURE__SURFACE_H
#define WARSTAGE__GESTURE__SURFACE_H

#include <vector>

class Gesture;
class Viewport;


class Surface {
    friend class Gesture;
    
    Viewport* viewport_;
    std::vector<Gesture*> gestures_{};

public:
    explicit Surface(Viewport& viewport) : viewport_{&viewport} {}
    
    [[nodiscard]] Viewport& GetViewport() const { return *viewport_; }
    
    [[nodiscard]] const std::vector<Gesture*>& GetGestures() const {
        return gestures_;
    }
};


#endif
