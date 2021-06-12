// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__BATTLE_MODEL__IMAGE_TILES_H
#define WARSTAGE__BATTLE_MODEL__IMAGE_TILES_H

#include "image/image.h"

class ImageTiles {
    glm::ivec2 imageSize_{};
    glm::ivec2 tileSize_{};
    glm::ivec2 tileCount_{};
    std::vector<std::unique_ptr<Image>> tiles_{};

public:
    ImageTiles(glm::ivec2 imageSize, int tileSize);

    void Save(const Image& image, bounds2i bounds);
    void Swap(const Image& image);

    int TileIndex(glm::ivec2 tilePos) const;
    bounds2i TileBounds(glm::ivec2 tilePos) const;
};


#endif
