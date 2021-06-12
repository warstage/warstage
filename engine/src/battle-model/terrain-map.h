// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_MODEL_TERRAIN_MAP_H
#define WARSTAGE__BATTLE_MODEL_TERRAIN_MAP_H

#include "./height-map.h"
#include "./image-tiles.h"
#include "geometry/bounds.h"
#include <string>

class Image;

enum class TerrainFeature { Hills, Water, Trees, Fords };


class TerrainMap {
public:
    bounds2f bounds_;
    HeightMap heightMap_;
    std::unique_ptr<Image> image_;
    std::unique_ptr<Image> height_;
    std::unique_ptr<Image> woods_;
    std::unique_ptr<Image> water_;
    std::unique_ptr<Image> fords_;
    std::unique_ptr<ImageTiles> imageTiles_;

public:
    static TerrainMap* getBlankMap();

    explicit TerrainMap(bounds2f bounds);
    TerrainMap(bounds2f bounds,
            std::unique_ptr<Image>&& height,
            std::unique_ptr<Image>&& woods,
            std::unique_ptr<Image>&& water,
            std::unique_ptr<Image>&& fords);

    [[nodiscard]] bounds2f getBounds() const { return heightMap_.getBounds(); }
    [[nodiscard]] Image* getImage() const { return image_.get(); }
    [[nodiscard]] const HeightMap& getHeightMap() const { return heightMap_; }

    [[nodiscard]] float calculateHeight(int x, int y) const;

    [[nodiscard]] bool isForest(glm::vec2 position) const;
    [[nodiscard]] bool isImpassable(glm::vec2 position) const;
    [[nodiscard]] bool containsWater(bounds2f bounds) const;

    [[nodiscard]] glm::ivec2 toImageCoordinates(glm::vec2 position) const { return toImageCoordinates(*height_, position); }
    [[nodiscard]] glm::ivec2 toImageCoordinates(const Image& image, glm::vec2 position) const;
    [[nodiscard]] int getForestValue(int x, int y) const;
    [[nodiscard]] float getImpassableValue(int x, int y) const;

    void extract(TerrainFeature feature, glm::vec2 position, Image& brush);
    bounds2f paint(TerrainFeature feature, glm::vec2 position, float pressure, const Image& brush);
    bounds2f paint(TerrainFeature feature, glm::vec2 position, float pressure, float radius);

    void prepareImageTiles();
    std::unique_ptr<ImageTiles> finishImageTiles();
    void swapImageTiles(ImageTiles& imageTiles, TerrainFeature feature);

private:
    static int terrainFeatureToPlane_(TerrainFeature feature);
};


#endif
