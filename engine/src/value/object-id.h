// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__VALUE__OBJECT_ID_H
#define WARSTAGE__VALUE__OBJECT_ID_H

#include <cstring>
#include <memory>
#include <mutex>


class ObjectId {
    friend class Value;

    static std::int32_t counter_;
    static std::mutex mutex_;
    char value_[12];

public:
    static ObjectId None;

    constexpr ObjectId() noexcept : value_{} {}
    explicit ObjectId(const void* value) { std::memcpy(value_, value ?: None.data(), 12); }

    static ObjectId create() noexcept;
    static ObjectId parse(const char* value) noexcept;

    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string debug_str() const;

    explicit operator bool() const noexcept { return *this != None; }
    bool operator !() const noexcept { return *this == None; }

    [[nodiscard]] const void* data() const noexcept { return value_; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return 12; }

    bool operator==(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) == 0; }
    bool operator!=(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) != 0; }
    bool operator<(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) < 0; }
    bool operator>(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) > 0; }
    bool operator<=(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) <= 0; }
    bool operator>=(const ObjectId& other) const { return std::memcmp(value_, other.value_, 12) >= 0; }
};


namespace std {
    
    template <>
    struct hash<ObjectId>
    {
        std::size_t operator()(const ObjectId& value) const
        {
            std::size_t result = 0;
            auto src = reinterpret_cast<const unsigned char*>(value.data());
            auto dst = reinterpret_cast<unsigned char*>(&result);
            std::size_t j = 0;
            for (std::size_t i = 0; i < value.size(); ++i) {
                dst[j] ^= src[i];
                if (++j == sizeof(std::size_t))
                    j = 0;
            }
            return result;
        }
    };
    
}


#endif
