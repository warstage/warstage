// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__BUILDER_H
#define WARSTAGE__VALUE__BUILDER_H

#include "./buffer.h"
#include "./value.h"
#include <cassert>
#include <memory>
#include <vector>


struct Struct {};
struct Array {};
struct ValueEnd {};

struct ValueBuilder {
    std::shared_ptr<ValueBuffer> buffer_{};
    explicit ValueBuilder(std::shared_ptr<ValueBuffer> buffer) :
        buffer_{std::move(buffer)} {
    }
    Value end_() {
        return Value{std::move(buffer_)};
    }
};

struct ArrayValueBuilder {
    std::shared_ptr<ValueBuffer> buffer_{};
    ArrayValueBuilder() : buffer_{std::make_shared<ValueBuffer>()} {
        buffer_->add_byte(static_cast<char>(ValueType::_array));
        buffer_->add_byte('_');
        buffer_->add_byte(0);
    }
    Value end_() {
        const char* ptr = reinterpret_cast<const char*>(buffer_->data());
        const char* end = ptr + buffer_->size();
        return Value{std::move(buffer_), ptr, end};
    }
};

template<typename T>
struct StructBuilder {
    ValueBuffer* buffer_;
    std::size_t start_;
    T parent_;
    int level_;

    StructBuilder(ValueBuffer* buffer, T parent) :
        buffer_{buffer}, start_{buffer_->size()}, parent_{parent} {
        level_ = buffer_->level_++;
        buffer_->add_int32(0);
    }

    T parent() {
        buffer_ = nullptr;
        return parent_;
    }

    auto end() -> decltype(parent_.end_()) {
        assert(--buffer_->level_ == level_);
        buffer_->add_byte(0);
        buffer_->set_int32(start_, buffer_->diff(start_));
        buffer_ = nullptr;
        return parent_.end_();
    }

    StructBuilder<T> end_() {
        return *this;
    }
};

inline StructBuilder<ValueBuilder> build_document() {
    auto buffer = std::make_shared<ValueBuffer>();
    auto buffer_ptr = buffer.get();
    return StructBuilder<ValueBuilder>{buffer_ptr, ValueBuilder{std::move(buffer)}};
}

template<typename T>
struct MemberBuilder {
    ValueBuffer* buffer_;
    T parent_;
    const char* name_;

    MemberBuilder(ValueBuffer* buffer, T parent, const char* name) :
        buffer_{buffer}, parent_{parent}, name_{name} {
    }

    T _next() {
        buffer_ = nullptr;
        return parent_;
    }
};


template<typename T>
struct ArrayBuilder {
    ValueBuffer* buffer_;
    std::size_t start_;
    T parent_;
    int index_{};
    int level_{};

    ArrayBuilder(const ArrayBuilder&) = default;

    ArrayBuilder(ValueBuffer* buffer, T parent) :
        buffer_{buffer}, start_{buffer_->size()}, parent_{parent} {
        level_ = buffer_->level_++;
        buffer_->add_int32(0);
    }

    ArrayBuilder<T> _next() {
        ArrayBuilder<T> next(*this);
        ++next.index_;
        buffer_ = nullptr;
        return next;
    }

    auto end() -> decltype(parent_.end_()) {
        assert(--buffer_->level_ == level_);
        buffer_->add_byte(0);
        buffer_->set_int32(start_, buffer_->diff(start_));
        buffer_ = nullptr;
        return parent_.end_();
    }

    ArrayBuilder<T> end_() {
        return *this;
    }
};

inline ArrayBuilder<ArrayValueBuilder> build_array() {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)};
}


/* Document << End */

inline Value operator<<(Struct, ValueEnd) {
    auto buffer = std::make_shared<ValueBuffer>();
    auto buffer_ptr = buffer.get();
    return StructBuilder<ValueBuilder>{buffer_ptr, ValueBuilder{std::move(buffer)}}.end();
}

template<typename T> auto operator<<(StructBuilder<T>&& builder, ValueEnd) -> decltype(builder.end()) {
    assert(builder.buffer_);
    return builder.end();
}

/* Array << End */

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, ValueEnd) -> decltype(builder.end()) {
    assert(builder.buffer_);
    return builder.end();
}

inline Value operator<<(Array, ValueEnd) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << ValueEnd{};
}


/* Document << Member */

inline MemberBuilder<StructBuilder<ValueBuilder>> operator<<(Struct, const char* member) {
    auto buffer = std::make_shared<ValueBuffer>();
    auto buffer_ptr = buffer.get();
    auto builder = StructBuilder<ValueBuilder>{buffer_ptr, ValueBuilder{std::move(buffer)}};
    return MemberBuilder<StructBuilder<ValueBuilder>>{buffer_ptr, builder, member};
}

template<typename T> MemberBuilder<StructBuilder<T>> operator<<(StructBuilder<T>&& builder, const char* member) {
    assert(builder.buffer_);
    return MemberBuilder<StructBuilder<T>>{builder.buffer_, builder, member};
}

/* Value */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, const Value& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    if (value.type() != ValueType::_undefined) {
        auto& buffer = *builder.buffer_;
        buffer.add_byte(static_cast<char>(value.type()));
        buffer.add_string(builder.name_);
        buffer.add_value(value);
    }
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, const Value& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    if (value.type() != ValueType::_undefined) {
        auto& buffer = *builder.buffer_;
        buffer.add_byte(static_cast<char>(value.type()));
        buffer.add_index(builder.index_);
        buffer.add_value(value);
    }
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const Value& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* Double */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, double value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_double));
    buffer.add_string(builder.name_);
    buffer.add_double(value);
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, double value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_double));
    buffer.add_index(builder.index_);
    buffer.add_double(value);
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, double value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* String */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, const char* value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    if (value) {
        buffer.add_byte(static_cast<int>(ValueType::_string));
        buffer.add_string(builder.name_);
        buffer.add_int32(static_cast<std::int32_t>(std::strlen(value)) + 1);
        buffer.add_string(value);
    } else {
        buffer.add_byte(static_cast<int>(ValueType::_null));
        buffer.add_string(builder.name_);
    }
    return builder._next();
}

template<typename T> auto operator<<(MemberBuilder<T>&& builder, const std::string& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_string));
    buffer.add_string(builder.name_);
    buffer.add_int32(static_cast<std::int32_t>(std::strlen(value.c_str())) + 1);
    buffer.add_string(value.c_str());
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, const char* value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    if (value) {
        buffer.add_byte(static_cast<int>(ValueType::_string));
        buffer.add_index(builder.index_);
        buffer.add_int32(static_cast<std::int32_t>(std::strlen(value)) + 1);
        buffer.add_string(value);
    } else {
        buffer.add_byte(static_cast<int>(ValueType::_null));
        buffer.add_index(builder.index_);
    }
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, const std::string& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_string));
    buffer.add_index(builder.index_);
    buffer.add_int32(static_cast<std::int32_t>(std::strlen(value.c_str())) + 1);
    buffer.add_string(value.c_str());
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const char* value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const std::string& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* Document */

template<typename T> StructBuilder<T> operator<<(MemberBuilder<T>&& builder, Struct) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_document));
    buffer.add_string(builder.name_);
    return StructBuilder<T>{builder.buffer_, builder._next()};
}

template<typename T> StructBuilder<ArrayBuilder<T>> operator<<(ArrayBuilder<T>&& builder, Struct) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_document));
    buffer.add_index(builder.index_);
    return StructBuilder<ArrayBuilder<T>>{builder.buffer_, builder._next()};
}

//inline array_builder<array_value_builder> operator<<(array, document value) {
//    array_value_builder builder{};
//    auto buffer = builder._buffer.get();
//    return array_builder<array_value_builder>{buffer, std::move(builder)} << value;
//}

/* Array */

template<typename T> ArrayBuilder<T> operator<<(MemberBuilder<T>&& builder, Array) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_array));
    buffer.add_string(builder.name_);
    return ArrayBuilder<T>{builder.buffer_, builder._next()};
}

template<typename T> ArrayBuilder<ArrayBuilder<T>> operator<<(ArrayBuilder<T>&& builder, Array) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_array));
    buffer.add_index(builder.index_);
    return ArrayBuilder<ArrayBuilder<T>>{builder.buffer_, builder._next()};
}

//inline array_builder<array_value_builder> operator<<(array, array value) {
//    array_value_builder builder{};
//    auto buffer = builder._buffer.get();
//    return array_builder<array_value_builder>{buffer, std::move(builder)} << value;
//}

/* ObjectId */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, const ObjectId& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    if (value != ObjectId::None) {
        buffer.add_byte(static_cast<int>(ValueType::_ObjectId));
        buffer.add_string(builder.name_);
        buffer.add_binary(value.data(), value.size());
    } else {
        buffer.add_byte(static_cast<int>(ValueType::_null));
        buffer.add_string(builder.name_);
    }
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, const ObjectId& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    if (value != ObjectId::None) {
        buffer.add_byte(static_cast<int>(ValueType::_ObjectId));
        buffer.add_index(builder.index_);
        buffer.add_binary(value.data(), value.size());
    } else {
        buffer.add_byte(static_cast<int>(ValueType::_null));
        buffer.add_index(builder.index_);
    }
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const ObjectId& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* Binary */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, const Binary& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_binary));
    buffer.add_string(builder.name_);
    buffer.add_int32(static_cast<std::int32_t>(value.size));
    buffer.add_byte(0);
    buffer.add_binary(value.data, value.size);
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, const Binary& value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_binary));
    buffer.add_index(builder.index_);
    buffer.add_int32(static_cast<std::int32_t>(value.size));
    buffer.add_byte(0);
    buffer.add_binary(value.data, value.size);
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const Binary& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* Bool */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, bool value)  -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_boolean));
    buffer.add_string(builder.name_);
    buffer.add_byte(static_cast<char>(value ? 1 : 0));
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, bool value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_boolean));
    buffer.add_index(builder.index_);
    buffer.add_byte(static_cast<char>(value ? 1 : 0));
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, bool value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* Int32 */

template<typename T> auto operator<<(MemberBuilder<T>&& builder, std::int32_t value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_int32));
    buffer.add_string(builder.name_);
    buffer.add_int32(value);
    return builder._next();
}

template<typename T> auto operator<<(ArrayBuilder<T>&& builder, std::int32_t value) -> decltype(builder._next()) {
    assert(builder.buffer_);
    auto& buffer = *builder.buffer_;
    buffer.add_byte(static_cast<int>(ValueType::_int32));
    buffer.add_index(builder.index_);
    buffer.add_int32(value);
    return builder._next();
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, std::int32_t value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* glm::vec2 */

template <typename T> auto operator<<(MemberBuilder<T>&& builder, const glm::vec2& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "0" << value.x
        << "1" << value.y
        << ValueEnd{};
}

template <typename T> auto operator<<(ArrayBuilder<T>&& builder, const glm::vec2& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "0" << value.x
        << "1" << value.y
        << ValueEnd{};
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const glm::vec2& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* glm::vec3 */

template <typename T> auto operator<<(MemberBuilder<T>&& builder, const glm::vec3& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "z" << value.z
        << "0" << value.x
        << "1" << value.y
        << "2" << value.z
        << ValueEnd{};
}

template <typename T> auto operator<<(ArrayBuilder<T>&& builder, const glm::vec3& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "z" << value.z
        << "0" << value.x
        << "1" << value.y
        << "2" << value.z
        << ValueEnd{};
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const glm::vec3& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* glm::vec4 */

template <typename T> auto operator<<(MemberBuilder<T>&& builder, const glm::vec4& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "z" << value.z
        << "w" << value.w
        << "0" << value.x
        << "1" << value.y
        << "2" << value.z
        << "3" << value.w
        << ValueEnd{};
}

template <typename T> auto operator<<(ArrayBuilder<T>&& builder, const glm::vec4& value) -> decltype(builder._next()) {
    return std::move(builder) << Struct{}
        << "x" << value.x
        << "y" << value.y
        << "z" << value.z
        << "w" << value.w
        << "0" << value.x
        << "1" << value.y
        << "2" << value.z
        << "3" << value.w
        << ValueEnd{};
}

inline ArrayBuilder<ArrayValueBuilder> operator<<(Array, const glm::vec4& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* std::vector */

template <typename T, typename V> auto operator<<(MemberBuilder<T>&& builder, const std::vector<V>& value) -> decltype(builder._next()) {
    auto arr = std::move(builder) << Array{};
    for (const V& v : value)
        arr = std::move(arr) << v;
    return std::move(arr) << ValueEnd{};
}

template <typename T, typename V> auto operator<<(ArrayBuilder<T>&& builder, const std::vector<V>& value) -> decltype(builder._next()) {
    auto arr = std::move(builder) << Array{};
    for (const V& v : value)
        arr = std::move(arr) << v;
    return std::move(arr) << ValueEnd{};
}

template <typename V> ArrayBuilder<ArrayValueBuilder> operator<<(Array, const std::vector<V>& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

/* std::array */

template <typename T, typename V, std::size_t N> auto operator<<(MemberBuilder<T>&& builder, const std::array<V, N>& value) -> decltype(builder._next()) {
    auto arr = std::move(builder) << Array{};
    for (const V& v : value)
        arr = std::move(arr) << v;
    return std::move(arr) << ValueEnd{};
}

template <typename T, typename V, std::size_t N> auto operator<<(ArrayBuilder<T>&& builder, const std::array<V, N>& value) -> decltype(builder._next()) {
    auto arr = std::move(builder) << Array{};
    for (const V& v : value)
        arr = std::move(arr) << v;
    return std::move(arr) << ValueEnd{};
}

template <typename V, std::size_t N> ArrayBuilder<ArrayValueBuilder> operator<<(Array, const std::array<V, N>& value) {
    ArrayValueBuilder builder{};
    auto buffer = builder.buffer_.get();
    return ArrayBuilder<ArrayValueBuilder>{buffer, std::move(builder)} << value;
}

#endif
