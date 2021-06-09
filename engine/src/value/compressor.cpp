// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./compressor.h"


void ValueCompressor::encode(const Value& value) {
    buffer_.clear();
    for (auto& v : value) {
        write(v, v.name());
    }
    add_byte(0x00u);
}


void ValueCompressor::write(const Value& value, const char* property_name) {
    auto property_id = property_name ? get_or_add_property_id(property_name) : 0x8000u;
    unsigned char header = (property_id & 0x100u) ? 0x80u : 0x00u;
    switch (value.type()) {
        case ValueType::_null: {
            header |= 0x01u;
            add_byte(header);
            add_property(property_id, property_name);
            break;
        }
        case ValueType::_boolean: {
            header |= value._bool() ? 0x03u : 0x02u;
            add_byte(header);
            add_property(property_id, property_name);
            break;
        }
        case ValueType::_document: {
            header |= 0x04u;
            add_byte(header);
            add_property(property_id, property_name);
            for (auto& v : value) {
                write(v, v.name());
            }
            add_byte(0x00u);
            break;
        }
        case ValueType::_array: {
            header |= 0x05u;
            add_byte(header);
            add_property(property_id, property_name);
            for (auto& v : value) {
                write(v, nullptr);
            }
            add_byte(0x00u);
            break;
        }
        case ValueType::_double: {
            header |= 0x06u;
            float v = value._float();
            add_byte(header);
            add_property(property_id, property_name);
            add_binary(&v, sizeof(float));
            break;
        }
        case ValueType::_ObjectId: {
            header |= 0x08u;
            std::uint32_t v = get_or_add_object_id(value._ObjectId());
            header |= (v & 0x700u) >> 8u;
            add_byte(header);
            add_property(property_id, property_name);
            add_byte(v & 0xffu);
            if (v == 0 || v == 0x7ff) {
                add_binary(value.data(), value.size());
            }
            break;
        }
        case ValueType::_int32: {
            std::uint32_t v = value._int32();
            if (v >= 0 && v < 24) {
                header |= 0x20u;
                header |= v;
                add_byte(header);
                add_property(property_id, property_name);
            } else {
                header |= 0x38u;
                if ((v & 0x80000000u) != 0) {
                    header |= 0x04u;
                    v ^= 0xffffffffu;
                }
                if (v < 0x100u) {
                    add_byte(header);
                    add_property(property_id, property_name);
                    add_byte(v & 0xffu);
                } else if (v < 0x10000u) {
                    header |= 0x01u;
                    add_byte(header);
                    add_property(property_id, property_name);
                    add_uint16(v);
                } else {
                    header |= 0x02u;
                    add_byte(header);
                    add_property(property_id, property_name);
                    add_uint32(v);
                }
            }
            break;
        }
        case ValueType::_binary: {
            auto v = value._binary();
            if (v.size != 0 && v.size < 0x1f) {
                header |= 0x40u;
                header |= v.size;
                add_byte(header);
                add_property(property_id, property_name);
                add_binary(v.data, v.size);
            } else if (v.size < 0x10000) {
                header |= 0x40u;
                add_byte(header);
                add_property(property_id, property_name);
                add_uint16(static_cast<unsigned int>(v.size));
                add_binary(v.data, v.size);
            } else /* if (v.size < 0x100000000) */ {
                header |= 0x5fu;
                add_byte(header);
                add_property(property_id, property_name);
                add_uint32(static_cast<unsigned int>(v.size));
                add_binary(v.data, v.size);
            }
            break;
        }
        case ValueType::_string: {
            const char* v = value._c_str();
            std::size_t length = std::strlen(v);
            if (length != 0 && length < 0x20) {
                header |= 0x60u;
                header |= length;
                add_byte(header);
                add_property(property_id, property_name);
                add_binary(v, length);
            } else {
                header |= 0x60u;
                add_byte(header);
                add_property(property_id, property_name);
                add_binary(v, length);
                add_byte(0);
            }
            break;
        }
        case ValueType::_undefined:
            break;
    }
}


void ValueCompressor::add_byte(unsigned char value) {
    buffer_.push_back(static_cast<char>(value));
}


void ValueCompressor::add_uint16(unsigned int value) {
    add_byte((value >> 8u) & 0xffu);
    add_byte(value & 0xffu);
}


void ValueCompressor::add_uint32(unsigned int value) {
    add_byte((value >> 24u) & 0xffu);
    add_byte((value >> 16u) & 0xffu);
    add_byte((value >> 8u) & 0xffu);
    add_byte(value & 0xffu);
}


void ValueCompressor::add_property(std::uint16_t value, const char* property_name) {
    if (property_name) {
        add_byte(value & 0xffu);
        if (value == 0 || value == 0x1ff) {
            add_string(property_name);
            add_byte(0);
        }
    }
}


void ValueCompressor::add_string(const char* value) {
    buffer_.append(value);
}


void ValueCompressor::add_binary(const void* data, std::size_t size) {
    buffer_.append(reinterpret_cast<const char*>(data), size);
}


std::uint16_t ValueCompressor::get_or_add_property_id(const char* property_name){
    if (auto p = properties_.FindValue(property_name)) {
        return *p;
    }
    auto result = 0;
    if (++lastPropertyId_ == 0x1ffu) {
        properties_.Clear();
        lastPropertyId_ = 1;
        result = 0x1ffu;
    }
    properties_.Value(property_name, false) = lastPropertyId_;
    return result;
}


std::uint16_t ValueCompressor::get_or_add_object_id(ObjectId value) {
    auto i = objects_.find(value);
    if (i != objects_.end()) {
        return i->second;
    }
    auto result = 0;
    if (++lastObjectId_ == 0x7ffu) {
        objects_.clear();
        lastObjectId_ = 1;
        result = 0x7ffu;
    }
    objects_[value] = lastObjectId_;
    return result;
}
