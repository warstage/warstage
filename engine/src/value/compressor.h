// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__COMPRESSOR_H
#define WARSTAGE__VALUE__COMPRESSOR_H

#include "./value.h"
#include "./dictionary.h"
#include <unordered_map>


// 0xxx xxxx, 0000 0000, <c-string> - added property
// 1xxx xxxx, 1111 1111, <c-string> - reset property
// pxxx xxxx, pppp pppp, - existing property P

// 0000 0000 - end of array/document
// p000 0001 - null
// p000 0010 - false
// p000 0011 - true

// p000 0100 - document
// p000 0101 - array
// p000 0110 - float
// p000 0111 - double (not used)

// p000 1000, pppp pppp, 0000 0000, <object id> - added object id
// p000 1111, pppp pppp, 1111 1111, <object id> - reset object id
// p000 1nnn, pppp pppp, nnnn nnnn - existing object id N

// ints (i: 0=normal, 1=inverted)
// p010 nnnn - int (0 - 15)
// p011 0nnn - int (16 - 23)
// p011 1i00, <1 byte>
// p011 1i01, <2 bytes>
// p011 1i10, <4 bytes>
// p011 1i11, <8 bytes> (not used)

// p100 0000 - binary (2 byte size)
// p10s ssss - binary (size = sssss)
// p101 1111 - binary (4 byte size)

// p110 0000 - string (nul terminated)
// p11s ssss - string (size = sssss)


class ValueCompressor {
    Dictionary<std::uint16_t> properties_{};
    std::unordered_map<ObjectId, std::uint16_t> objects_{};
    std::uint16_t lastPropertyId_{};
    std::uint16_t lastObjectId_{};
    std::string buffer_{};

public:
    void encode(const Value &value);

    [[nodiscard]] const void* data() const { return buffer_.data(); }
    [[nodiscard]] std::size_t size() const { return buffer_.size(); }

private:
    void write(const Value &value, const char *propertyName);

    void add_byte(unsigned char value);
    void add_uint16(unsigned int value);
    void add_uint32(unsigned int value);
    void add_string(const char* value);
    void add_binary(const void* data, std::size_t size);

    void add_property(std::uint16_t value, const char* property_name);

    std::uint16_t get_or_add_property_id(const char *propertyName);
    std::uint16_t get_or_add_object_id(ObjectId value);

};

#endif
