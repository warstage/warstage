// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__IMAGE__IMAGE_H
#define WARSTAGE__IMAGE__IMAGE_H

#include "geometry/bounds.h"
#include <functional>
#include <vector>
#include <glm/glm.hpp>


class Image {
public:
    const glm::ivec3 size_{};
    const glm::ivec3 next_{};
    const std::shared_ptr<std::uint8_t> data_{};

    Image() = default;
    explicit Image(glm::ivec3 size);
    Image(glm::ivec3 size, std::shared_ptr<std::uint8_t> data);
    Image(glm::ivec3 size, glm::ivec3 next, std::shared_ptr<std::uint8_t> data);

    Image(const Image&) = default;
    Image(Image&&) = default;
    Image& operator=(const Image&) = delete;
    Image& operator=(Image&&) = delete;

    static std::unique_ptr<Image> decodePng(const void* data, std::size_t size);

    [[nodiscard]] Image subImage(bounds3i bounds) const;

    template <typename T>
    void applyFilter(T filter) {
        std::uint8_t data[4];
        auto p1 = data_.get();
        for (int y = 0; y < size_.y; ++y) {
            auto p2 = p1;
            for (int x = 0; x < size_.x; ++x) {
                auto p3 = p2;
                for (int z = 0; z < size_.z; ++z) {
                    data[z] = *p3;
                    p3 += next_.z;
                }
                filter(data);
                p3 = p2;
                for (int z = 0; z < size_.z; ++z) {
                    *p3 = data[z];
                    p3 += next_.z;
                }
                p2 += next_.x;
            }
            p1 += next_.y;
        }
    }
    template <typename T>
    void applyFilterXY(T filter) {
        std::uint8_t data[4];
        auto p1 = data_.get();
        for (int y = 0; y < size_.y; ++y) {
            auto p2 = p1;
            for (int x = 0; x < size_.x; ++x) {
                auto p3 = p2;
                for (int z = 0; z < size_.z; ++z) {
                    data[z] = *p3;
                    p3 += next_.z;
                }
                filter(data, x, y);
                p3 = p2;
                for (int z = 0; z < size_.z; ++z) {
                    *p3 = data[z];
                    p3 += next_.z;
                }
                p2 += next_.x;
            }
            p1 += next_.y;
        }
    }

    template <typename T>
    void applyImage(const Image& image, T filter) {
        assert(image.size_ == size_);
        std::uint8_t src[4];
        std::uint8_t img[4];
        auto p1 = data_.get();
        auto q1 = const_cast<const std::uint8_t*>(image.data_.get());
        for (int y = 0; y < size_.y; ++y) {
            auto p2 = p1;
            auto q2 = q1;
            for (int x = 0; x < size_.x; ++x) {
                auto p3 = p2;
                auto q3 = q2;
                for (int z = 0; z < size_.z; ++z) {
                    src[z] = *p3;
                    img[z] = *q3;
                    p3 += next_.z;
                    q3 += image.next_.z;
                }
                filter(src, const_cast<const std::uint8_t*>(img));
                p3 = p2;
                q3 = q2;
                for (int z = 0; z < size_.z; ++z) {
                    *p3 = src[z];
                    p3 += next_.z;
                    q3 += image.next_.z;
                }
                p2 += next_.x;
                q2 += image.next_.x;
            }
            p1 += next_.y;
            q1 += image.next_.y;
        }
    }
    template <typename T>
    void applyImageXY(const Image& image, T filter) {
        assert(image.size_ == size_);
        std::uint8_t src[4];
        std::uint8_t img[4];
        auto p1 = data_.get();
        auto q1 = const_cast<const std::uint8_t*>(image.data_.get());
        for (int y = 0; y < size_.y; ++y) {
            auto p2 = p1;
            auto q2 = q1;
            for (int x = 0; x < size_.x; ++x) {
                auto p3 = p2;
                auto q3 = q2;
                for (int z = 0; z < size_.z; ++z) {
                    src[z] = *p3;
                    img[z] = *q3;
                    p3 += next_.z;
                    q3 += image.next_.z;
                }
                filter(src, const_cast<const std::uint8_t*>(img), x, y);
                p3 = p2;
                q3 = q2;
                for (int z = 0; z < size_.z; ++z) {
                    *p3 = src[z];
                    p3 += next_.z;
                    q3 += image.next_.z;
                }
                p2 += next_.x;
                q2 += image.next_.x;
            }
            p1 += next_.y;
            q1 += image.next_.y;
        }
    }

    [[nodiscard]] int getValue(glm::ivec3 p) const;
    [[nodiscard]] glm::vec4 getPixel(int x, int y) const;
    void setPixel(int x, int y, glm::vec4 c);

    void premultiplyAlpha();
};


#endif
