// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_GESTURES__EDITOR_MODEL_H
#define WARSTAGE__BATTLE_GESTURES__EDITOR_MODEL_H

#include "battle-model/terrain-map.h"
#include "battle-model/image-tiles.h"
#include "image/image.h"
#include "runtime/runtime.h"

class UnitController;
class TerrainMap;

enum class EditorMode { Hand, Paint, Erase, Smear };

class EditorObserver {
public:
    virtual ~EditorObserver();
    virtual void OnTerrainChanged(TerrainFeature terrainFeature, bounds2f bounds) = 0;
};


class EditorModel : public std::enable_shared_from_this<EditorModel> {
    UnitController* unitController_{};
    EditorObserver* editorObserver_{};
    Image brush_{};
    Image mixer_{};
    glm::vec2 brushPosition_{};

    ObjectRef editorObject_{};

    std::shared_ptr<Federate> systemFederate_{};

    std::vector<std::pair<TerrainFeature, std::unique_ptr<ImageTiles>>> undo_{};
    std::vector<std::pair<TerrainFeature, std::unique_ptr<ImageTiles>>> redo_{};

public:
    EditorModel(Runtime& runtime, UnitController* unitController, EditorObserver* editorObserver, ObjectRef editorObject);

    void Initialize();
    void ResetUndoList();

    TerrainMap* getTerrainMap() const;
    EditorMode GetEditorMode() const;
    TerrainFeature GetTerrainFeature() const;

    void ToolBegan(glm::vec2 position);
    void ToolMoved(glm::vec2 position);
    void ToolEnded(glm::vec2 position);
    void ToolCancelled();

private:
    void Undo();
    void Redo();

    void UpdateCommandButtons();

    void Paint(TerrainFeature feature, glm::vec2 position, bool value);

    void SmearReset(TerrainFeature feature, glm::vec2 position);
    void SmearPaint(TerrainFeature feature, glm::vec2 position);

    void Swap(TerrainFeature feature, ImageTiles& imageTiles);
};


#endif
