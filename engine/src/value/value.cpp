// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#include "./buffer.h"
#include "./value.h"
#include <ctime>
#include <thread>
#include <random>


ObjectId ObjectId::None;
std::int32_t ObjectId::counter_;
std::mutex ObjectId::mutex_;

void ValueBuffer::add_index(int value) {
    if (0 < value && value <= 999999) {
        char buffer[10];
        int length = 0;
        while (value) {
            buffer[length++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
        for (int i = 0; i < length; ++i)
            value_.push_back(buffer[i]);
    } else {
        value_.push_back('0');
    }
    value_.push_back('\0');
}


void ValueBuffer::add_binary(const void* data, std::size_t size) {
    if (data) {
        value_.append(static_cast<const char*>(data), size);
    } else if (size) {
        value_.append(size, '\0');
    }
}


void ValueBuffer::add_value(const Value& value) {
    value_.append(reinterpret_cast<const char*>(value.data()), value.size());
}


double ValueBuffer::get_double(const void* data) {
    double value;
    std::memcpy(&value, data, 8);
    return value;
}


std::int32_t ValueBuffer::get_int32(const void* data) {
    std::int32_t value;
    std::memcpy(&value, data, 4);
    return value;
}


ObjectId ObjectId::create() noexcept {
    std::lock_guard lock{mutex_};
    
    std::int32_t value[3];
    
    std::time_t t = std::time(nullptr);
    value[0] = static_cast<std::int32_t>(t);
    
    std::hash<std::thread::id> hasher;
    value[1] = static_cast<std::int32_t>(hasher(std::this_thread::get_id()));
    
    if (!counter_) {
        std::random_device r;
        counter_ = static_cast<std::int32_t>(r());
    }
    value[2] = counter_++;
    
    return ObjectId(value);
}


static unsigned char decode_hex(char c) {
    return '0' <= c && c <= '9' ? c - '0' : 10u + static_cast<unsigned char>(c - 'a') & 15u;
}


ObjectId ObjectId::parse(const char* value) noexcept {
    if (!value)
        return ObjectId{};
    unsigned char buffer[12];
    const char* end = value + 24;
    unsigned char* p = buffer;
    while (value != end) {
        if (value[0] == '\0' || value[1] == '\0')
            return ObjectId{};
        *p++ = static_cast<unsigned char>(decode_hex(value[0]) << 4u) | decode_hex(value[1]);
        value += 2;
    }
    return ObjectId{buffer};
}


std::string ObjectId::str() const {
    char hex[17] = "0123456789abcdef";
    std::string result(24, ' ');
    int i = 0;
    int j = 0;
    while (i < 12) {
        result[j++] = hex[static_cast<unsigned char>(value_[i]) >> 4u];
        result[j++] = hex[static_cast<unsigned char>(value_[i++]) & 15u];
    }
    return result;
}


std::string ObjectId::debug_str() const {
    if (*this == ObjectId{})
        return "0000";
    if (*this == ObjectId::parse("000000000000000000000001"))
        return "0001";
    if (*this == ObjectId::parse("ffffffffffffffffffffffff"))
        return "ffff";
    
    char hex[17] = "0123456789abcdef";
    auto hash = std::hash<std::string>{}(str());
    auto p = reinterpret_cast<unsigned char*>(&hash);
    auto q = p + sizeof(hash);
    unsigned char result[5] = "\0\0\0\0";
    int i = 0;
    while (p != q) {
        result[i] ^= static_cast<unsigned char>(*p >> 4u);
        result[i] ^= static_cast<unsigned char>(*p++ & 15u);
        if (++i == 4)
            i = 0;
    }
    for (i = 0; i < 4; ++i) {
        result[i] = hex[result[i]];
    }
    return reinterpret_cast<char*>(result);
}


ValueBase::ValueBase(const char* ptr, const char* end) {
    //assert(ptr && end);
    if (ptr && ptr < end && *ptr) {
        const char* data = ptr + 1 + std::strlen(ptr + 1) + 1;
        const char* next = ValueElement::find_next(static_cast<ValueType>(*ptr), data);
        if (data <= end && next) {
            ptr_ = ptr;
            end_ = end;
            data_ = data;
            next_ = next <= end ? next : end;
            //if (_next != next)
            //    log("bson next invalid");
            // NOTE: Some bson libs skip trailing document zeros.
            // So make sure _next doesn't overflow the buffer
        }
    }
}


Value::Value(const ValueElement& e) {
    assert(e.bufptr_);
    buffer_ = *e.bufptr_;
    ptr_ = e.ptr_;
    end_ = e.end_;
    data_ = e.data_;
    next_ = e.next_;
}


Value::Value(std::shared_ptr<ValueBuffer> buffer) {
    buffer_ = std::move(buffer);
    if (buffer_) {
        auto size = static_cast<std::size_t>(ValueBuffer::get_int32(buffer_->data()));
        if (buffer_->size() < size) {
            // NOTE: Some bson libs skip trailing document zeros.
            // Extend the buffer with trailing zeros.
            auto s = std::string(size, char{});
            std::memcpy(&s[0], buffer_->data(), buffer_->size());
            buffer_ = std::shared_ptr<ValueBuffer>{new ValueBuffer{std::move(s)}};
        }
        data_ = reinterpret_cast<const char*>(buffer_->data());
        next_ = end_ = data_ + buffer_->size();
    }
}


Value::Value(std::shared_ptr<ValueBuffer> buffer, const char* ptr, const char* end) {
    assert(buffer && ptr && end);
    buffer_ = std::move(buffer);
    if (ptr && ptr < end && *ptr) {
        const char* data = ptr + 1 + std::strlen(ptr + 1) + 1;
        const char* next = ValueElement::find_next(static_cast<ValueType>(*ptr), data);
        if (data <= end && next) {
            ptr_ = ptr;
            end_ = end;
            data_ = data;
            next_ = next <= end ? next : end;
            //if (_next != next)
            //    log("bson next invalid");
            // NOTE: Some bson libs skip trailing document zeros.
            // So make sure _next doesn't overflow the buffer
        }
    }
}


ValueIterator ValueBase::begin() const {
    switch (type()) {
        case ValueType::_document:
        case ValueType::_array:
            return ValueIterator{&buffer_, data_ + 4, end_};
        default:
            return ValueIterator{&buffer_, nullptr, nullptr};
    }
}


ValueIterator ValueBase::end() const {
    switch (type()) {
        case ValueType::_document:
        case ValueType::_array:
            return ValueIterator{&buffer_, end_, end_};
        default:
            return ValueIterator{&buffer_, nullptr, nullptr};
    }
}


ValueElement ValueBase::operator[](const char* name) const {
    for (auto i = begin(), e = end(); i != e; ++i)
        if (std::strcmp(name, i->name()) == 0)
            return *i;
    
    return ValueElement{&buffer_, end_, end_};
}


bool operator==(const ValueBase& x, const ValueBase& y) {
    return x.type() == y.type()
        && x.size() == y.size()
        && std::memcmp(x.data(), y.data(), x.size()) == 0;
}


bool operator!=(const ValueBase& x, const ValueBase& y) {
    return !(x == y);
}


template <> bool bson_decode<bool>(const ValueBase& value, bool*) {
    switch (value.type()) {
        case ValueType::_boolean:
            return *reinterpret_cast<const char*>(value.data()) != 0;
        case ValueType::_double:
            return ValueBuffer::get_double(value.data()) != 0;
        case ValueType::_int32:
            return ValueBuffer::get_int32(value.data()) != 0;
        case ValueType::_string:
            return *(reinterpret_cast<const char*>(value.data()) + 4) != '\0';
        case ValueType::_array:
        case ValueType::_document:
        case ValueType::_ObjectId:
        case ValueType::_binary:
            return true;
        default:
            return false;
    }
}


template <> double bson_decode<double>(const ValueBase& value, double*) {
    switch (value.type()) {
        case ValueType::_boolean:
            return *reinterpret_cast<const char*>(value.data()) ? 1 : 0;
        case ValueType::_double:
            return ValueBuffer::get_double(value.data());
        case ValueType::_int32:
            return ValueBuffer::get_int32(value.data());
        default:
            return 0.0;
    }
}


template <> std::int32_t bson_decode<std::int32_t>(const ValueBase& value, std::int32_t*) {
    switch (value.type()) {
        case ValueType::_boolean:
            return *reinterpret_cast<const char*>(value.data()) ? 1 : 0;
        case ValueType::_double:
            return (std::int32_t)ValueBuffer::get_double(value.data());
        case ValueType::_int32:
            return ValueBuffer::get_int32(value.data());
        default:
            return 0;
    }
}


template <> const char* bson_decode<const char*>(const ValueBase& value, const char**) {
    return value.is_string() ? reinterpret_cast<const char*>(value.data()) + 4 : nullptr;
}


template <> ObjectId bson_decode<ObjectId>(const ValueBase& value, ObjectId*) {
    return value.is_ObjectId() ? ObjectId{value.data()} : ObjectId{};
}


template <> Binary bson_decode<Binary>(const ValueBase& value, Binary*) {
    return value.is_binary() ? Binary{
        reinterpret_cast<const char*>(value.data()) + 5,
        static_cast<std::size_t>(ValueBuffer::get_int32(value.data()))
    } : Binary{};
}


ValueIterator ValueElement::begin() const {
    auto t = type();
    if (t == ValueType::_document || t == ValueType::_array)
        return ValueIterator{bufptr_, data_ + 4, end_};
    return ValueIterator{bufptr_, nullptr, nullptr};
}


ValueIterator ValueElement::end() const {
    auto t = type();
    if (t == ValueType::_document || t == ValueType::_array)
        return ValueIterator{bufptr_, end_, end_};
    return ValueIterator{bufptr_, nullptr, nullptr};
}


ValueElement ValueElement::operator[](const char* name) const {
    for (auto i = begin(), e = end(); i != e; ++i)
        if (std::strcmp(name, i->name()) == 0)
            return *i;
    
    return ValueElement{&buffer_, end_, end_};
}


const char* ValueElement::find_next(ValueType type, const char* data) {
    switch (type) {
        case ValueType::_null:
            return data;
        
        case ValueType::_boolean:
            return data + 1;
        
        case ValueType::_int32:
            return data + 4;
        
        case ValueType::_double:
            return data + 8;
        
        case ValueType::_string:
            return data + 4 + static_cast<std::uint32_t>(ValueBuffer::get_int32(data));
        
        case ValueType::_binary:
            return data + 5 + static_cast<std::uint32_t>(ValueBuffer::get_int32(data));
        
        case ValueType::_document:
        case ValueType::_array:
            return data + static_cast<std::uint32_t>(ValueBuffer::get_int32(data));
        
        case ValueType::_ObjectId:
            return data + 12;
        
        default:
            return nullptr;
    }
}
