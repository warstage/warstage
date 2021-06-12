// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./camera-control.h"
#include "./editor-gesture.h"
#include "./editor-model.h"
#include "battle-gestures/unit-controller.h"
#include "gesture/pointer.h"


EditorGesture::EditorGesture(Surface* gestureSurface, UnitController* unitController, std::shared_ptr<EditorModel> editorModel)
    : Gesture{gestureSurface},
    unitController_{unitController},
    editorModel_{std::move(editorModel)} {
}


void EditorGesture::PointerWillBegin(Pointer* pointer) {
    SubscribePointer(pointer);
}


void EditorGesture::PointerHasBegan(Pointer* pointer) {
    if (pointer->IsCaptured() || HasCapturedPointer())
        return;

    if (editorModel_->GetEditorMode() == EditorMode::Hand)
        return;

    if (TryCapturePointer(pointer)) {
        if (unitController_->acquireTerrainMap()) {
            editorModel_->ToolBegan(TerrainPosition(pointer));
            StartInterval();
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorGesture::PointerWasMoved(Pointer* pointer) {
    if (HasCapturedPointer(pointer)) {
        if (unitController_->acquireTerrainMap()) {
            auto p0 = pointer->GetPreviousPosition();
            auto p1 = pointer->GetCurrentPosition();
            if (glm::distance(p0, p1) > 2.0f) {
                editorModel_->ToolMoved(TerrainPosition(pointer));
            }
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorGesture::PointerWasEnded(Pointer* pointer) {
    if (HasCapturedPointer(pointer)) {
        if (unitController_->acquireTerrainMap()) {
            editorModel_->ToolEnded(TerrainPosition(pointer));
            ReleasePointer(pointer);
            StopInterval();
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorGesture::PointerWasCancelled(Pointer* pointer) {
    if (HasCapturedPointer(pointer)) {
        if (unitController_->acquireTerrainMap()) {
            editorModel_->ToolCancelled();
            ReleasePointer(pointer);
            StopInterval();
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorGesture::StartInterval() {
    if (!interval_) {
        auto weak_ = weak_from_this();
        interval_ = Strand::getMain()->setInterval([weak_]() {
          if (auto this_ = weak_.lock()) {
            if (this_->HasCapturedPointer()) {
              if (this_->unitController_->acquireTerrainMap()) {
                auto p = this_->TerrainPosition(this_->GetCapturedPointer());
                this_->editorModel_->ToolMoved(p);
              }
                this_->unitController_->releaseTerrainMap();
            }
          }
        }, 1000.0f / 20.0f);
    }
}


void EditorGesture::StopInterval() {
    if (interval_) {
      clearInterval(*interval_);
        interval_ = nullptr;
    }
}


glm::vec2 EditorGesture::TerrainPosition(Pointer* pointer) {
    return unitController_->GetCameraControl()->GetTerrainPosition3(pointer->GetCurrentPosition()).xy();
}
