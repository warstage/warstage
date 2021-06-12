// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./image-tiles.h"


ImageTiles::ImageTiles(glm::ivec2 imageSize, int tileSize) :
    imageSize_{imageSize},
    tileSize_{tileSize},
    tileCount_{(imageSize + tileSize - 1) / tileSize} {
    tiles_.resize(static_cast<std::size_t>(tileCount_.x * tileCount_.y));
};


void ImageTiles::Save(const Image& image, bounds2i bounds) {
    auto min = bounds.min / tileSize_;
    auto max = (bounds.max + tileSize_ - 1) / tileSize_;
    for (int x = min.x; x < max.x; ++x)
        for (int y = min.y; y < max.y; ++y) {
            int index = TileIndex({x, y});
            if (!tiles_[index]) {
                auto img = image.subImage(bounds3i{TileBounds({x, y}), 0, image.size_.z});
                tiles_[index] = std::unique_ptr<Image>{new Image{glm::vec3{tileSize_, image.size_.z}}};
                tiles_[index]->applyImage(img, [](std::uint8_t* p, const std::uint8_t* q) {
                    *p = *q;
                });
            }
        }
}


void ImageTiles::Swap(const Image& image) {
    auto tmp = Image{glm::vec3{tileSize_, image.size_.z}};
    for (int x = 0; x < tileCount_.x; ++x)
        for (int y = 0; y < tileCount_.y; ++y) {
            int index = TileIndex({x, y});
            if (tiles_[index]) {
                auto img = image.subImage(bounds3i{TileBounds({x, y}), 0, image.size_.z});
                tmp.applyImage(img, [](std::uint8_t* p, const std::uint8_t* q) { *p = *q; });
                img.applyImage(*tiles_[index], [](std::uint8_t* p, const std::uint8_t* q) { *p = *q; });
                tiles_[index]->applyImage(tmp, [](std::uint8_t* p, const std::uint8_t* q) { *p = *q; });
            }
        }
}


int ImageTiles::TileIndex(glm::ivec2 tilePos) const {
    return tilePos.y * tileCount_.x + tilePos.x;
}


bounds2i ImageTiles::TileBounds(glm::ivec2 tilePos) const {
    auto min = tileSize_ * tilePos;
    return bounds2i{min, min + tileSize_};
}
