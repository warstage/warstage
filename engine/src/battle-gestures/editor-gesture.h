// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__EDITOR_GESTURE_H
#define WARSTAGE__BATTLE_GESTURES__EDITOR_GESTURE_H

#include "async/strand.h"
#include "gesture/gesture.h"

class UnitController;
class EditorModel;


class EditorGesture : public std::enable_shared_from_this<EditorGesture>, public Gesture {
    UnitController* unitController_;
    std::shared_ptr<EditorModel> editorModel_;
    std::shared_ptr<IntervalObject> interval_;

public:
    EditorGesture(Surface* gestureSurface, UnitController* unitController, std::shared_ptr<EditorModel> editorModel);

    void PointerWillBegin(Pointer* pointer) override;
    void PointerHasBegan(Pointer* pointer) override;
    void PointerWasMoved(Pointer* pointer) override;
    void PointerWasEnded(Pointer* pointer) override;
    void PointerWasCancelled(Pointer* pointer) override;

private:
    void StartInterval();
    void StopInterval();

    glm::vec2 TerrainPosition(Pointer* pointer);
};


#endif
