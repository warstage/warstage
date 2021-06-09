// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__BUFFER_H
#define WARSTAGE__VALUE__BUFFER_H

#include <cstring>
#include <memory>
#include <string>


class Value;

enum class ValueType : unsigned char {
    _undefined = 0x00,
    _string = 0x02,
    _document = 0x03,
    _array = 0x04,
    _binary = 0x05,
    _ObjectId = 0x07,
    _boolean = 0x08,
    _null = 0x0a,
    _int32 = 0x10,
    _double = 0x01,
};


struct Binary {
    const void* data;
    std::size_t size;
    Binary() : data{}, size{} {}
    Binary(const void* d, std::size_t s) : data(d), size(s) {}
    explicit Binary(const std::string& s) : data(s.data()), size(s.size()) {}
};


class ValueBuffer {
public:
    std::string value_;
    int level_{};

    ValueBuffer() = default;
    explicit ValueBuffer(std::string s) : value_(std::move(s)) {}
    ValueBuffer(const void* data, std::size_t size) : value_{reinterpret_cast<const char*>(data), size} {}

    [[nodiscard]] const void* data() const { return value_.data(); }
    [[nodiscard]] std::size_t size() const { return value_.size(); }
    [[nodiscard]] std::int32_t diff(std::size_t start) const { return static_cast<std::int32_t>(value_.size() - start); }

    void add_byte(char value){
        value_.push_back(value);
    }
    void add_int32(std::int32_t value){
        value_.append(reinterpret_cast<const char*>(&value), 4);
    }
    void add_double(double value){
        value_.append(reinterpret_cast<const char*>(&value), 8);
    }
    void add_string(const char* value){
        value_.append(value);
        value_.push_back('\0');
    }
    void add_index(int value);
    void add_binary(const void* data, std::size_t size);
    void add_value(const Value& value);

    void set_int32(std::size_t offset, std::int32_t value){
        std::memcpy(&value_[offset], &value, 4);
    }

    static double get_double(const void* data);
    static std::int32_t get_int32(const void* data);
};

#endif
