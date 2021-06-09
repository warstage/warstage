// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__DECOMPRESSOR_H
#define WARSTAGE__VALUE__DECOMPRESSOR_H

#include "./value.h"
#include <string>


class ValueDecompressor {
    std::vector<std::size_t> propertyLookup_{};
    std::string propertyStrings_{};
    std::vector<ObjectId> objectIds_{};
    const unsigned char* ptr_{};
    const unsigned char* end_{};
    ValueBuffer buffer_{};
    std::string index_{};
    bool failure_{};

public:
    bool decode(const void* data, std::size_t size);

    const void* data() const { return buffer_.data(); }
    std::size_t size() const { return buffer_.size(); }

private:
    bool decode_element(bool is_property, int index);

    const char* read_property(std::uint16_t header);
    const char* make_index(int index);

    unsigned char read_byte();
    std::uint16_t read_uint16();
    std::uint32_t read_uint32();
    float read_float();
};

#endif
