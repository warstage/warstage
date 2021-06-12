// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./terrain-map.h"
#include "image/image.h"


TerrainMap* TerrainMap::getBlankMap() {
    static TerrainMap* _instance;
    if (!_instance)
        _instance = new TerrainMap{{0.0f, 0.0f, 1024.0f, 1024.0f}};
    return _instance;
}


TerrainMap::TerrainMap(bounds2f bounds) :
        bounds_{bounds},
        heightMap_{bounds} {
    heightMap_.update({256, 256}, [this](int x, int y) { return calculateHeight(x, y); });
}

TerrainMap::TerrainMap(bounds2f bounds,
        std::unique_ptr<Image>&& height,
        std::unique_ptr<Image>&& woods,
        std::unique_ptr<Image>&& water,
        std::unique_ptr<Image>&& fords) :
        bounds_{bounds},
        heightMap_{bounds},
        image_{},
        height_{std::move(height)},
        woods_{std::move(woods)},
        water_{std::move(water)},
        fords_{std::move(fords)} {
    heightMap_.update({256, 256}, [this](int x, int y) { return calculateHeight(x, y); });
}


float TerrainMap::calculateHeight(int x, int y) const {
    if (!height_)
        return 2.0f;

    if (x < 1) x = 1; else if (x > height_->size_.xy().x - 1) x = height_->size_.xy().x - 1;
    if (y < 1) y = 1; else if (y > height_->size_.xy().y - 1) y = height_->size_.xy().y - 1;

    int height_xy = height_->getValue({x, y, 0});
    int height_xn = height_->getValue({x - 1, y, 0});
    int height_xp = height_->getValue({x + 1, y, 0});
    int height_yn = height_->getValue({x, y - 1, 0});
    int height_yp = height_->getValue({x, y + 1, 0});

    float height_value = 0.5f * static_cast<float>(height_xy) + 0.125f * static_cast<float>(height_xn + height_xp + height_yn + height_yp);

    float height = (0.5f + 124.5f * (1.0f - height_value / 255.0f));

    float water = static_cast<float>(water_->getValue({x, y, 0})) / 255.0f;
    height = glm::mix(height, -2.5f, water);

    float fords = static_cast<float>(fords_->getValue({x, y, 0})) / 255.0f;
    height = glm::mix(height, -0.5f, water * fords);

    return height;
}


bool TerrainMap::isForest(glm::vec2 position) const {
    if (woods_) {
        auto coord = toImageCoordinates(*woods_, position);
        return getForestValue(coord.x, coord.y) >= 128;
    }
    return false;
}


bool TerrainMap::isImpassable(glm::vec2 position) const {
    auto coord = toImageCoordinates(*height_, position);
    return getImpassableValue(coord.x, coord.y) >= 0.5;
}


bool TerrainMap::containsWater(bounds2f bounds) const {
    if (water_) {
        auto mapsize = water_->size_.xy();
        auto min = glm::vec2{mapsize.x - 1, mapsize.y - 1} * (bounds.min - bounds_.min) / bounds_.size();
        auto max = glm::vec2{mapsize.x - 1, mapsize.y - 1} * (bounds.max - bounds_.min) / bounds_.size();
        int xmin = static_cast<int>(floorf(min.x));
        int ymin = static_cast<int>(floorf(min.y));
        int xmax = static_cast<int>(ceilf(max.x));
        int ymax = static_cast<int>(ceilf(max.y));

        for (int x = xmin; x <= xmax; ++x)
            for (int y = ymin; y <= ymax; ++y) {
                auto w = water_->getValue({x, y, 0});
                if (w >= 128)
                    return true;
            }
    }
    return false;
}


glm::ivec2 TerrainMap::toImageCoordinates(const Image& image, glm::vec2 position) const {
    auto p = (position - bounds_.min) / bounds_.size();
    auto s = image.size_.xy();
    return glm::ivec2{static_cast<int>(p.x * s.x), static_cast<int>(p.y * s.y)};
}


int TerrainMap::getForestValue(int x, int y) const {
    return woods_ ? woods_->getValue({x, y, 0}) : 0;
}


float TerrainMap::getImpassableValue(int x, int y) const {
    if (0 <= x && x < 255 && 0 <= y && y < 255) {
        const auto water = water_ ? water_->getValue({x, y, 0}) : 0;
        const auto fords = fords_ ? fords_->getValue({x, y, 0}) : 0;
        if (water >= 128 && fords < 128)
            return 1.0f;
    }
    auto n = heightMap_.getNormal(x, y);
    return bounds1f{0, 1}.clamp(0.5f + 8.0f * (0.83f - n.z));
}


void TerrainMap::extract(TerrainFeature feature, glm::vec2 position, Image& brush) {
    if (image_) {
        auto size = brush.size_.xy();
        auto origin = toImageCoordinates(*image_, position) - size / 2;
        auto image_bounds = bounds2i{origin, origin + size};
        auto clamp_bounds = bounds2i{0, 0, image_->size_}.clamp(image_bounds);
        auto brush_bounds = bounds3i{clamp_bounds - image_bounds.min, 0, 1};
        int plane = terrainFeatureToPlane_(feature);
        auto image = image_->subImage({clamp_bounds, plane, plane + 1});

        brush.subImage(brush_bounds).applyImage(image, [](std::uint8_t* p, const std::uint8_t* q) {
            *p = *q;
        });
    }
}


bounds2f TerrainMap::paint(TerrainFeature feature, glm::vec2 position, float pressure, const Image& brush) {
    if (image_) {
        float brush_radius = brush.size_.x / 2.0f;
        float radius = brush_radius * bounds_.size().x / static_cast<float>(image_->size_.x);
        auto center = toImageCoordinates(*image_, position);
        auto image_bounds = bounds2i{center}.set_size(brush.size_);
        auto clamp_bounds = bounds2i{0, 0, image_->size_}.clamp(image_bounds);
        auto brush_bounds = bounds3i{clamp_bounds - image_bounds.min, 0, 1};
        int plane = terrainFeatureToPlane_(feature);
        auto image = image_->subImage({clamp_bounds, plane, plane + 1});

        if (imageTiles_)
            imageTiles_->Save(image_->subImage({bounds2i{0, 0, image_->size_}, plane, plane + 1}), clamp_bounds);

        auto mid = clamp_bounds.size() / 2;
        float scale = 12.0f / brush_radius;
        image.applyImageXY(brush.subImage(brush_bounds), [mid, scale, pressure](std::uint8_t* p, const std::uint8_t* q, int x, int y) {
            float d = 6.0f - scale * glm::length(glm::vec2{x - mid.x, y - mid.y});
            float k = 1.0f / (1.0f + glm::exp(-d)) - 0.01f;
            if (k > 0.0f) {
                float c = glm::mix((float) *p, (float) *q, k * pressure);
                c = glm::clamp(c, 0.0f, 255.0f);
                *p = static_cast<std::uint8_t>(glm::round(c));
            }
        });

        heightMap_.update({256, 256}, [this](int x, int y) { return calculateHeight(x, y); });

        return bounds2f(position).add_radius(radius + 1);
    }
    return bounds2f{position};
}


bounds2f TerrainMap::paint(TerrainFeature feature, glm::vec2 position, float pressure, float radius) {
    if (image_) {
        float brush_radius = radius * static_cast<float>(image_->size_.x) / bounds_.size().x;
        float abs_pressure = glm::abs(pressure);
        auto center = toImageCoordinates(*image_, position);
        auto image_bounds = bounds2i{center}.add_radius(static_cast<int>(brush_radius));
        auto clamp_bounds = bounds2i{0, 0, image_->size_}.clamp(image_bounds);
        int plane = terrainFeatureToPlane_(feature);
        auto image = image_->subImage({clamp_bounds, plane, plane + 1});

        if (imageTiles_)
            imageTiles_->Save(image_->subImage({bounds2i{0, 0, image_->size_}, plane, plane + 1}), clamp_bounds);

        auto mid = clamp_bounds.size() / 2;
        float scale = 12.0f / brush_radius;
        if (feature == TerrainFeature::Hills) {
            float delta = pressure > 0 ? -5.0f : 5.0f;
            image.applyFilterXY([mid, scale, delta, abs_pressure](std::uint8_t* p, int x, int y) {
                float d = 6.0f - glm::length(glm::vec2{x - mid.x, y - mid.y}) * scale;
                float k = 1.0f / (1.0f + glm::exp(-d)) - 0.1f;
                if (k > 0.0f) {
                    float c = *p;
                    c = glm::mix(c, c + delta, k * abs_pressure);
                    c = glm::clamp(c, 0.0f, 255.0f);
                    *p = static_cast<std::uint8_t>(glm::round(c));
                }
            });
        } else {
            float value = pressure > 0 ? 255.0f : 0.0f;
            image.applyFilterXY([mid, scale, value, abs_pressure](std::uint8_t* p, int x, int y) {
                float d = 6.0f - glm::length(glm::vec2{x - mid.x, y - mid.y}) * scale;
                float k = 1.0f / (1.0f + glm::exp(-d)) - 0.1f;
                if (k > 0.0f) {
                    float c = *p;
                    c = glm::mix(c, value, k * abs_pressure);
                    *p = static_cast<std::uint8_t>(glm::round(c));
                }
            });
        }

        heightMap_.update({256, 256}, [this](int x, int y) { return calculateHeight(x, y); });

        return bounds2f(position).add_radius(radius + 1);
    }
    return bounds2f{position};
}


void TerrainMap::prepareImageTiles() {
    if (image_) {
        auto size = image_->size_.xy();
        imageTiles_ = std::unique_ptr<ImageTiles>{new ImageTiles{size, 16}};
    }
}


std::unique_ptr<ImageTiles> TerrainMap::finishImageTiles() {
    return std::move(imageTiles_);
}


void TerrainMap::swapImageTiles(ImageTiles& imageTiles, TerrainFeature feature) {
    if (image_) {
        int plane = terrainFeatureToPlane_(feature);
        auto bounds = bounds3i{bounds2i{0, 0, image_->size_}, plane, plane + 1};
        imageTiles.Swap(image_->subImage(bounds));
        heightMap_.update({256, 256}, [this](int x, int y) { return calculateHeight(x, y); });
    }
}

int TerrainMap::terrainFeatureToPlane_(TerrainFeature feature) {
    switch (feature) {
        case TerrainFeature::Hills: return 3;
        case TerrainFeature::Trees: return 1;
        case TerrainFeature::Water: return 2;
        case TerrainFeature::Fords: return 0;
    }
}

