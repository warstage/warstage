// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./image.h"
#include "./lodepng.h"
#include <cstdlib>


Image::Image(glm::ivec3 size) :
        size_{size},
        next_{size.z, size.x * size.z, 1},
        data_{
                static_cast<std::uint8_t*>(std::calloc(static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y), static_cast<std::size_t>(size.z))),
                [](std::uint8_t* data) {
                    std::free(data);
                }
        } {
}


Image::Image(glm::ivec3 size, std::shared_ptr<std::uint8_t> data) :
        size_{size},
        next_{size.z, size.x * size.z, 1},
        data_{std::move(data)} {
}


Image::Image(glm::ivec3 size, glm::ivec3 next, std::shared_ptr<std::uint8_t> data) :
        size_{size},
        next_{next},
        data_{std::move(data)} {
}


std::unique_ptr<Image> Image::decodePng(const void* data, std::size_t size) {
    std::vector<unsigned char> out{};
    unsigned width, height;
    unsigned error = lodepng::decode(out, width, height, reinterpret_cast<const unsigned char*>(data), size, LCT_RGBA, 8);
    if (error)
        return nullptr;

    auto result = std::make_unique<Image>(glm::ivec3{width, height, 4});
    std::memcpy(result->data_.get(), out.data(), out.size());

    return result;
}


Image Image::subImage(bounds3i bounds) const {
    assert(0 <= bounds.min.x && bounds.max.x <= size_.x);
    assert(0 <= bounds.min.y && bounds.max.y <= size_.y);
    assert(0 <= bounds.min.z && bounds.max.z <= size_.z);

    auto data = data_;
    auto p = data.get()
            + bounds.min.x * next_.x
            + bounds.min.y * next_.y
            + bounds.min.z * next_.z;

    return Image{bounds.size(), next_, std::shared_ptr<std::uint8_t>{p, [data](auto){ }}};
}


int Image::getValue(glm::ivec3 p) const {
    return glm::all(glm::lessThanEqual({}, p)) && glm::all(glm::lessThan(p, size_))
            ? data_.get()[p.x * next_.x + p.y * next_.y + p.z * next_.z]
            : 0;
}


glm::vec4 Image::getPixel(int x, int y) const {
    if (0 <= x && x < size_.x && 0 <= y && y < size_.y) {
        auto p = data_.get() + x * next_.x + y * next_.y;
        constexpr float k = 1.0f / 255.0f;
        return {
                k * static_cast<float>(p[0]),
                k * static_cast<float>(p[1]),
                k * static_cast<float>(p[2]),
                k * static_cast<float>(p[3])
        };
    }
    return {};
}


void Image::setPixel(int x, int y, glm::vec4 c) {
    if (0 <= x && x < size_.x && 0 <= y && y < size_.y) {
        auto bounds = bounds1f{0.0f, 255.0f};
        auto p = data_.get() + x * next_.x + y * next_.y;
        p[0] = static_cast<unsigned char>(glm::round(bounds.clamp(c.r * 255)));
        p[1] = static_cast<unsigned char>(glm::round(bounds.clamp(c.g * 255)));
        p[2] = static_cast<unsigned char>(glm::round(bounds.clamp(c.b * 255)));
        p[3] = static_cast<unsigned char>(glm::round(bounds.clamp(c.a * 255)));
    }
}


void Image::premultiplyAlpha() {
    for (int y = 0; y < size_.y; ++y)
        for (int x = 0; x < size_.x; ++x) {
            auto c = getPixel(x, y);
            c.r *= c.a;
            c.g *= c.a;
            c.b *= c.a;
            setPixel(x, y, c);
        }
}
