// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./editor-model.h"
#include "./unit-controller.h"


EditorObserver::~EditorObserver() {
}


EditorModel::EditorModel(Runtime& runtime, UnitController* unitController, EditorObserver* editorObserver, ObjectRef editorObject) :
    unitController_{unitController},
    editorObserver_{editorObserver},
    brush_{glm::ivec3{32, 32, 1}},
    mixer_{glm::ivec3{32, 32, 1}},
    editorObject_{editorObject} {

    systemFederate_ = unitController->systemFederate_;
}


void EditorModel::Initialize() {
    auto weak_ = weak_from_this();

  systemFederate_->getEventClass("editor-undo").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock())
      this_->Undo();
  });

  systemFederate_->getEventClass("editor-redo").subscribe([weak_](const Value& params) {
    if (auto this_ = weak_.lock())
      this_->Redo();
  });
}


void EditorModel::ResetUndoList() {
    if (!undo_.empty() || !redo_.empty()) {
        if (unitController_->acquireTerrainMap()) {
            undo_.clear();
            redo_.clear();
            UpdateCommandButtons();
        }
        unitController_->releaseTerrainMap();
    }
}

TerrainMap* EditorModel::getTerrainMap() const {
    return const_cast<TerrainMap*>(&unitController_->getTerrainMap());
}


EditorMode EditorModel::GetEditorMode() const {
    return static_cast<EditorMode>(editorObject_["editorMode"_int]);
}


TerrainFeature EditorModel::GetTerrainFeature() const {
    return static_cast<TerrainFeature>(editorObject_["terrainFeature"_int]);
}


void EditorModel::ToolBegan(glm::vec2 position) {
    if (auto terrainMap = getTerrainMap()) {
        terrainMap->prepareImageTiles();
    }

    switch (GetEditorMode()) {
        case EditorMode::Smear:
            SmearReset(GetTerrainFeature(), position);
            break;

        case EditorMode::Paint:
            Paint(GetTerrainFeature(), position, 1);
            break;

        case EditorMode::Erase:
            Paint(GetTerrainFeature(), position, 0);
            break;

        default:
            break;
    }
}


void EditorModel::ToolMoved(glm::vec2 position) {
    switch (GetEditorMode()) {
        case EditorMode::Smear:
            SmearPaint(GetTerrainFeature(), position);
            break;

        case EditorMode::Paint:
            Paint(GetTerrainFeature(), position, 1);
            break;

        case EditorMode::Erase:
            Paint(GetTerrainFeature(), position, 0);
            break;

        default:
            break;
    }
}


void EditorModel::ToolEnded(glm::vec2 position) {
    switch (GetEditorMode()) {
        case EditorMode::Smear:
            SmearPaint(GetTerrainFeature(), position);
            break;

        case EditorMode::Paint:
            Paint(GetTerrainFeature(), position, 1);
            break;

        case EditorMode::Erase:
            Paint(GetTerrainFeature(), position, 0);
            break;

        default:
            break;
    }

    if (auto terrainMap = getTerrainMap()) {
        auto imageTiles = terrainMap->finishImageTiles();
        undo_.push_back(std::make_pair(GetTerrainFeature(), std::move(imageTiles)));
        redo_.clear();
        UpdateCommandButtons();
    }
}


void EditorModel::ToolCancelled() {
    if (auto terrainMap = getTerrainMap()) {
        auto imageTiles = terrainMap->finishImageTiles();
        Swap(GetTerrainFeature(), *imageTiles);
    }
}


void EditorModel::Undo() {
    if (!undo_.empty()) {
        if (unitController_->acquireTerrainMap()) {
            auto change = std::move(undo_.back());
            undo_.pop_back();
            Swap(change.first, *change.second);
            redo_.push_back(std::move(change));
            UpdateCommandButtons();
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorModel::Redo() {
    if (!redo_.empty()) {
        if (unitController_->acquireTerrainMap()) {
            auto change = std::move(redo_.back());
            redo_.pop_back();
            Swap(change.first, *change.second);
            undo_.push_back(std::move(change));
            UpdateCommandButtons();
        }
        unitController_->releaseTerrainMap();
    }
}


void EditorModel::UpdateCommandButtons() {
    editorObject_["canUndo"] = !undo_.empty();
    editorObject_["canRedo"] = !redo_.empty();
}


void EditorModel::Paint(TerrainFeature feature, glm::vec2 position, bool value) {
    float pressure = value ? 0.4f : -0.4f;
    float radius = feature == TerrainFeature::Hills ? 64.0f : 32.0f;
    if (auto terrainMap = getTerrainMap()) {
        auto bounds = terrainMap->paint(feature, position, pressure, radius);

        editorObserver_->OnTerrainChanged(feature, bounds);
    }
}


void EditorModel::SmearReset(TerrainFeature feature, glm::vec2 position) {
    if (auto terrainMap = getTerrainMap()) {
        terrainMap->extract(feature, position, brush_);
        brushPosition_ = position;
    }
}


void EditorModel::SmearPaint(TerrainFeature feature, glm::vec2 position) {
    if (auto terrainMap = getTerrainMap()) {
        bool done = false;
        while (!done) {
            auto delta = position - brushPosition_;
            float distance = glm::length(delta);
            if (distance > 4.0f) {
                brushPosition_ += (4.0f / distance) * delta;
            } else {
                brushPosition_ = position;
                done = true;
            }

            auto bounds = terrainMap->paint(feature, brushPosition_, 0.2f, brush_);
            editorObserver_->OnTerrainChanged(feature, bounds);

            terrainMap->extract(feature, brushPosition_, mixer_);
            brush_.applyImage(mixer_, [](std::uint8_t* p, const std::uint8_t* q) {
                float c = glm::mix(*p, *q, 0.5f);
                *p = static_cast<std::uint8_t>(glm::round(c));
            });
        }
    }
}


void EditorModel::Swap(TerrainFeature feature, ImageTiles& imageTiles) {
    if (auto terrainMap = getTerrainMap()) {
        terrainMap->swapImageTiles(imageTiles, feature);
        editorObserver_->OnTerrainChanged(feature, terrainMap->getBounds());
    }
}
