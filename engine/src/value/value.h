// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__VALUE_H
#define WARSTAGE__VALUE__VALUE_H

#include "./buffer.h"
#include "./object-id.h"
#include <glm/glm.hpp>


class ValueBase;

template <typename T> T bson_decode(const ValueBase& value, T*);

template <> bool bson_decode<bool>(const ValueBase& value, bool*);
template <> double bson_decode<double>(const ValueBase& value, double*);
template <> std::int32_t bson_decode<std::int32_t>(const ValueBase& value, std::int32_t*);
template <> const char* bson_decode<const char*>(const ValueBase& value, const char**);
template <> ObjectId bson_decode<ObjectId>(const ValueBase& value, ObjectId*);
template <> Binary bson_decode<Binary>(const ValueBase& value, Binary*);


class ValueElement;
class ValueIterator;
struct ValueProperty {};

template <typename T> struct ValueSymbol { const char* name; };

class ValueBase {
    friend class ValueBuffer;
    friend class ValueElement;
    friend class ValueIterator;
    friend class Value;

protected:
    std::shared_ptr<ValueBuffer> buffer_{};
    const char* ptr_{};
    const char* end_{};
    const char* data_{};
    const char* next_{};
    ValueBase(const char* ptr, const char* end);
public:
    ValueBase() = default;

    [[nodiscard]] ValueType type() const {
        return ptr_ ? static_cast<ValueType>(*ptr_)
            : data_ ? ValueType::_document
            : ValueType::_undefined;
    }

    [[nodiscard]] const void* data() const { return data_; }
    [[nodiscard]] std::size_t size() const { return next_ - data_; }

    [[nodiscard]] ValueIterator begin() const;
    [[nodiscard]] ValueIterator end() const;

    ValueElement operator[](const char* name) const;
    ValueElement operator[](const ValueSymbol<Value>& s) const;
    template <typename T> T operator[](const ValueSymbol<T>& s) const;

    [[nodiscard]] bool has_value() const { return type()  != ValueType::_undefined && type() != ValueType::_null; }
    [[nodiscard]] bool is_defined() const { return type() != ValueType::_undefined; }
    [[nodiscard]] bool is_undefined() const { return type() == ValueType::_undefined; }
    [[nodiscard]] bool is_double() const { return type() == ValueType::_double; }
    [[nodiscard]] bool is_string() const { return type() == ValueType::_string; }
    [[nodiscard]] bool is_document() const { return type() == ValueType::_document; }
    [[nodiscard]] bool is_array() const { return type() == ValueType::_array; }
    [[nodiscard]] bool is_ObjectId() const { return type() == ValueType::_ObjectId; }
    [[nodiscard]] bool is_binary() const { return type() == ValueType::_binary; }
    [[nodiscard]] bool is_boolean() const { return type() == ValueType::_boolean; }
    [[nodiscard]] bool is_null() const { return type() == ValueType::_null; }
    [[nodiscard]] bool is_int32() const { return type() == ValueType::_int32; }

    template <typename T> T cast() const {
        return ::bson_decode(*this, (T*){});
    }

    [[nodiscard]] float _float() const {
        return static_cast<float>(::bson_decode(*this, (double*){}));
    }
    [[nodiscard]] double _double() const {
        return ::bson_decode(*this, (double*){});
    }
    [[nodiscard]] const char* _c_str() const {
        return ::bson_decode(*this, (const char**){});
    }
    [[nodiscard]] ObjectId _ObjectId() const {
        return ::bson_decode(*this, (ObjectId*){});
    }
    [[nodiscard]] Binary _binary() const {
        return ::bson_decode(*this, (Binary*){});
    }
    [[nodiscard]] bool _bool() const {
        return ::bson_decode(*this, (bool*){});
    }
    [[nodiscard]] std::int32_t _int32() const {
        return ::bson_decode(*this, (std::int32_t*){});
    }
    [[nodiscard]] int _int() const {
        return ::bson_decode(*this, (int*){});
    }
    [[nodiscard]] glm::vec2 _vec2() const;
    [[nodiscard]] glm::vec3 _vec3() const;
    [[nodiscard]] glm::vec4 _vec4() const;
};

class Value : public ValueBase {
public:
    Value() = default;
    explicit Value(std::shared_ptr<ValueBuffer> buffer);
    Value(std::shared_ptr<ValueBuffer> buffer, const char* ptr, const char* end);

    Value(const Value&) = default;
    Value(const ValueElement& e);

    Value& operator=(const Value&) = default;
    Value& operator=(const ValueElement&) = delete;

};

class ValueElement : public ValueBase {
    friend class Value;
    friend class ValueBase;
    friend class ValueIterator;
    const std::shared_ptr<ValueBuffer>* bufptr_;
    ValueElement(const ValueElement& i) = default;
    ValueElement& operator=(const ValueElement&) = default;
public:
    ValueElement(const std::shared_ptr<ValueBuffer>* buf, const char* ptr, const char* end) : ValueBase{ptr, end}, bufptr_{buf} { }

    [[nodiscard]] const char* name() const { return ptr_ ? ptr_ + 1 : nullptr; }
    [[nodiscard]] ValueElement next() const { return ValueElement{bufptr_, next_, end_}; }

    [[nodiscard]] ValueIterator begin() const;
    [[nodiscard]] ValueIterator end() const;

    [[nodiscard]] ValueElement operator[](const char* name) const;
    [[nodiscard]] ValueElement operator[](const ValueSymbol<Value>& s) const;
    template <typename T> [[nodiscard]] T operator[](const ValueSymbol<T>& s) const;

private:
    static const char* find_next(ValueType type, const char* data);
};


class ValueIterator {
    ValueElement element_;
public:
    ValueIterator(const std::shared_ptr<ValueBuffer>* buf, const char* ptr, const char* end) : element_{buf, ptr, end} {}
    ValueIterator(const ValueIterator& i) = default;

    const ValueElement& operator*() const { return element_; }
    const ValueElement* operator->() const { return &element_; }

    const ValueIterator& operator++() {
        element_ = element_.next();
        return *this;
    }

    const ValueIterator operator++(int) {
        ValueIterator i = *this;
        ++(*this);
        return i;
    }

    bool operator==(const ValueIterator& i) const { return element_.ptr_ == i.element_.ptr_; }
    bool operator!=(const ValueIterator& i) const { return element_.ptr_ != i.element_.ptr_; }
};


bool operator==(const ValueBase& x, const ValueBase& y);
bool operator!=(const ValueBase& x, const ValueBase& y);

constexpr ValueSymbol<ValueProperty> inline operator "" _property(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<Value> inline operator "" _value(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<bool> inline operator "" _bool(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<int> inline operator "" _int(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<float> inline operator "" _float(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<double> inline operator "" _double(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<const char*> inline operator "" _c_str(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<std::string> inline operator "" _str(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<ObjectId> inline operator "" _ObjectId(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<Binary> inline operator "" _binary(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<glm::vec2> inline operator "" _vec2(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<glm::vec3> inline operator "" _vec3(const char* n, std::size_t) { return {n}; }
constexpr ValueSymbol<glm::vec4> inline operator "" _vec4(const char* n, std::size_t) { return {n}; }

template <typename T> inline T ValueBase::operator[](const ValueSymbol<T>& s) const {
    return ::bson_decode((*this)[s.name], (T*){});
}

template <typename T> inline T ValueElement::operator[](const ValueSymbol<T>& s) const {
    return ::bson_decode((*this)[s.name], (T*){});
}

template <> inline std::string ValueBase::operator[](const ValueSymbol<std::string>& s) const {
    return (*this)[s.name]._c_str() ?: "";
}

template <> inline std::string ValueElement::operator[](const ValueSymbol<std::string>& s) const {
    return (*this)[s.name]._c_str() ?: "";
}

inline ValueElement ValueBase::operator[](const ValueSymbol<Value>& s) const {
    return (*this)[s.name];
}

inline ValueElement ValueElement::operator[](const ValueSymbol<Value>& s) const {
    return (*this)[s.name];
}

inline glm::vec2 ValueBase::_vec2() const {
  switch (type()) {
    case ValueType::_array:
      return {
        (*this)["0"_float],
        (*this)["1"_float]
      };
    case ValueType::_document:
      return {
        (*this)["x"_float],
        (*this)["y"_float]
      };
    default:
      return {};
  }
}

inline glm::vec3 ValueBase::_vec3() const {
  switch (type()) {
    case ValueType::_array:
      return {
          (*this)["0"_float],
          (*this)["1"_float],
          (*this)["2"_float]
      };
    case ValueType::_document:
      return {
          (*this)["x"_float],
          (*this)["y"_float],
          (*this)["z"_float]
      };
    default:
      return {};
  }
}

inline glm::vec4 ValueBase::_vec4() const {
  switch (type()) {
    case ValueType::_array:
      return {
          (*this)["0"_float],
          (*this)["1"_float],
          (*this)["2"_float],
          (*this)["3"_float]
      };
    case ValueType::_document:
      return {
          (*this)["x"_float],
          (*this)["y"_float],
          (*this)["z"_float],
          (*this)["w"_float]
      };
    default:
      return {};
  };
}


template <> inline float bson_decode<float>(const ValueBase& value, float*) {
    return value._float();
}
template <> inline glm::vec2 bson_decode<glm::vec2>(const ValueBase& value, glm::vec2*) {
    return value._vec2();
}
template <> inline glm::vec3 bson_decode<glm::vec3>(const ValueBase& value, glm::vec3*) {
    return value._vec3();
}
template <> inline glm::vec4 bson_decode<glm::vec4>(const ValueBase& value, glm::vec4*) {
    return value._vec4();
}

#include "./builder.h"

#endif
