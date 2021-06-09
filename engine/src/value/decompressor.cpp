// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./decompressor.h"
#include <sstream>


bool ValueDecompressor::decode(const void* data, std::size_t size) {
    failure_ = false;
    
    buffer_.value_.clear();
    buffer_.level_ = 0;
    
    ptr_ = reinterpret_cast<const unsigned char*>(data);
    end_ = ptr_ + size;
    
    auto start = buffer_.size();
    buffer_.add_int32(0);
    while (decode_element(true, 0)) {
    }
    buffer_.add_byte(0);
    buffer_.set_int32(start, buffer_.diff(start));
    
    return !failure_;
}


bool ValueDecompressor::decode_element(bool is_property, int index) {
    unsigned char header = read_byte();
    if (header == 0) {
        return false;
    }
    
    unsigned char type = header & 0x7fu;
    
    const char* property_name = is_property ? read_property(header) : make_index(index);
    if (!property_name) {
        failure_ = true;
        return false;
    }
    
    switch (type) {
        case 0x01: /* null */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_null));
            buffer_.add_string(property_name);
            return true;
        }
        case 0x02: /* false */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_boolean));
            buffer_.add_string(property_name);
            buffer_.add_byte(0);
            return true;
        }
        case 0x03: /* true */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_boolean));
            buffer_.add_string(property_name);
            buffer_.add_byte(1);
            return true;
        }
        case 0x04: /* document */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_document));
            buffer_.add_string(property_name);
            auto start = buffer_.size();
            buffer_.add_int32(0);
            while (decode_element(true, 0)) {
            }
            buffer_.add_byte(0);
            buffer_.set_int32(start, buffer_.diff(start));
            return !failure_;
        }
        case 0x05: /* array */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_array));
            buffer_.add_string(property_name);
            auto start = buffer_.size();
            buffer_.add_int32(0);
            int i = 0;
            while (decode_element(false, i)) {
                ++i;
            }
            buffer_.add_byte(0);
            buffer_.set_int32(start, buffer_.diff(start));
            return !failure_;
        }
        case 0x06: /* float */ {
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_double));
            buffer_.add_string(property_name);
            buffer_.add_double(read_float());
            return true;
        }
        default:
            break;
    }
    
    if ((type & 0x78u) == 0x08) /* ObjectId */ {
        std::uint16_t obj = read_byte();
        obj |= (static_cast<std::uint16_t>(type) & 0x07u) << 8u;
        if (obj == 0x7ff) {
            objectIds_.clear();
            obj = 0;
        }
        ObjectId v;
        if (obj == 0) {
            if (ptr_ + 12 > end_) {
                failure_ = true;
                return false; // error
            }
            v = ObjectId{ptr_};
            ptr_ += 12;
            objectIds_.push_back(v);
        } else if (obj <= objectIds_.size())  {
            v = objectIds_[obj - 1];
        } else {
            failure_ = true;
            return false; // error
        }
        buffer_.add_byte(static_cast<unsigned char>(ValueType::_ObjectId));
        buffer_.add_string(property_name);
        buffer_.add_binary(v.data(), v.size());
        return true;
    }
    
    switch (type & 0x60u) {
        case 0x20u: /* int */ {
            std::uint32_t n = type & 0x1fu;
            if (n < 24) {
                buffer_.add_byte(static_cast<unsigned char>(ValueType::_int32));
                buffer_.add_string(property_name);
                buffer_.add_int32(n);
                return true;
            }
            std::uint32_t v = 0;
            switch (type & 0x03u) {
                case 0:
                    v = read_byte();
                    break;
                case 1:
                    v = read_uint16();
                    break;
                case 2:
                    v = read_uint32();
                    break;
            }
            if ((type & 0x04u) != 0) {
                v ^= 0xffffffffu;
            }

            buffer_.add_byte(static_cast<unsigned char>(ValueType::_int32));
            buffer_.add_string(property_name);
            buffer_.add_int32(v);
            return true;
        }
        case 0x40u: /* binary */ {
            std::uint32_t size = type & 0x1fu;
            if (size == 0) {
                size = read_uint16();
            } else if (size == 0x1fu) {
                size = read_uint32();
            }
            if (ptr_ + size > end_) {
                failure_ = true;
                return false; // error
            }
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_binary));
            buffer_.add_string(property_name);
            buffer_.add_int32(size);
            buffer_.add_byte(0);
            buffer_.add_binary(ptr_, size);
            ptr_ += size;
            return true;
        }
        case 0x60u: /* string */ {
            const char* data = reinterpret_cast<const char*>(ptr_);
            std::uint32_t size = type & 0x1fu;
            if (size == 0) {
                if (ptr_ + size + 1 > end_) {
                    failure_ = true;
                    return false; // error
                }
                size = std::strlen(data);
                ptr_ += size + 1;
            } else {
                if (ptr_ + size > end_) {
                    failure_ = true;
                    return false; // error
                }
                ptr_ += size;
            }
            buffer_.add_byte(static_cast<unsigned char>(ValueType::_string));
            buffer_.add_string(property_name);
            buffer_.add_int32(size + 1);
            buffer_.add_binary(data, size);
            buffer_.add_byte(0);
            return true;
        }
    }

    failure_ = true;
    return false;
}


const char* ValueDecompressor::read_property(std::uint16_t header) {
    std::uint16_t index = read_byte();
    if ((header & 0x80u) != 0) {
        index |= 0x100u;
    }
    if (index == 0x1ffu) {
        propertyStrings_.clear();
        propertyLookup_.clear();
        index = 0;
    }
    if (index == 0) {
        const char* result = reinterpret_cast<const char*>(ptr_);
        while (ptr_ != end_ && *ptr_ != '\0')
            ++ptr_;
        if (ptr_ == end_) {
            return "";
        }
        ++ptr_;
        auto s = propertyStrings_.size();
        propertyLookup_.push_back(s);
        propertyStrings_.append(result);
        propertyStrings_.push_back('\0');
        return result;
    }
    --index;
    if (index >= propertyLookup_.size()) {
        return nullptr;
    }
    auto i = propertyLookup_[index];
    if (i >= propertyStrings_.size()) {
        return nullptr;
    }
    return &propertyStrings_[i];
}


const char* ValueDecompressor::make_index(int index) {
    std::ostringstream os{};
    os << index;
    index_ = os.str();
    return index_.c_str();
}


unsigned char ValueDecompressor::read_byte() {
    return ptr_ < end_ ? *ptr_++ : 0;
}


std::uint16_t ValueDecompressor::read_uint16() {
    if (ptr_ + 2 > end_) {
        return 0; // error
    }
    std::uint16_t result
        = static_cast<std::uint16_t>(ptr_[0] << 8u)
        | static_cast<std::uint16_t>(ptr_[1]);
    ptr_ += 2;
    return result;
}


std::uint32_t ValueDecompressor::read_uint32() {
    if (ptr_ + 4 > end_) {
        return 0; // error
    }
    std::uint32_t result
        = static_cast<std::uint32_t>(ptr_[0] << 24u)
        | static_cast<std::uint32_t>(ptr_[1] << 16u)
        | static_cast<std::uint32_t>(ptr_[2] << 8u)
        | static_cast<std::uint32_t>(ptr_[3]);
    ptr_ += 4;
    return result;
}


float ValueDecompressor::read_float() {
    if (ptr_ + 4 > end_) {
        return 0; // error
    }
    float value;
    std::memcpy(&value, ptr_, sizeof(float));
    ptr_ += sizeof(float);
    return value;
}

