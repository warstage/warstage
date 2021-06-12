// Copyright Felix Ungman. All rights reserved.
// Licensed under GNU General Public License version 3 or later.

#ifndef WARSTAGE__GRAPHICS__VERTEX_H
#define WARSTAGE__GRAPHICS__VERTEX_H

#include "./graphics-api.h"
#include <glm/glm.hpp>
#include <vector>


template<class T> struct VertexAttributeSize;
template<> struct VertexAttributeSize<GLbyte> { static constexpr GLint value{1}; };
template<> struct VertexAttributeSize<GLubyte> { static constexpr GLint value{1}; };
template<> struct VertexAttributeSize<GLshort> { static constexpr GLint value{1}; };
template<> struct VertexAttributeSize<GLushort> { static constexpr GLint value{1}; };
template<> struct VertexAttributeSize<GLfloat> { static constexpr GLint value{1}; };
template<class T, glm::precision P> struct VertexAttributeSize<glm::tvec1<T, P>> { static constexpr GLint value{1}; };
template<class T, glm::precision P> struct VertexAttributeSize<glm::tvec2<T, P>> { static constexpr GLint value{2}; };
template<class T, glm::precision P> struct VertexAttributeSize<glm::tvec3<T, P>> { static constexpr GLint value{3}; };
template<class T, glm::precision P> struct VertexAttributeSize<glm::tvec4<T, P>> { static constexpr GLint value{4}; };


template<class T> struct VertexAttributeType;
template<> struct VertexAttributeType<GLbyte> { static constexpr GLenum value{GL_BYTE}; };
template<> struct VertexAttributeType<GLubyte> { static constexpr GLenum value{GL_UNSIGNED_BYTE}; };
template<> struct VertexAttributeType<GLshort> { static constexpr GLenum value{GL_SHORT}; };
template<> struct VertexAttributeType<GLushort> { static constexpr GLenum value{GL_UNSIGNED_SHORT}; };
template<> struct VertexAttributeType<GLfloat> { static constexpr GLenum value{GL_FLOAT}; };
template<class T, glm::precision P> struct VertexAttributeType<glm::tvec1<T, P>> { static constexpr GLenum value{VertexAttributeType<T>::value}; };
template<class T, glm::precision P> struct VertexAttributeType<glm::tvec2<T, P>> { static constexpr GLenum value{VertexAttributeType<T>::value}; };
template<class T, glm::precision P> struct VertexAttributeType<glm::tvec3<T, P>> { static constexpr GLenum value{VertexAttributeType<T>::value}; };
template<class T, glm::precision P> struct VertexAttributeType<glm::tvec4<T, P>> { static constexpr GLenum value{VertexAttributeType<T>::value}; };


struct VertexAttributeTraits {
    const GLchar* name;
    GLint size;
    GLenum type;
    GLintptr offset;
};

template <int Index, class Vertex>
static constexpr GLintptr getVertexAttributeOffset() {
    const Vertex* vertex = nullptr;
    const auto* p0 = reinterpret_cast<const char*>(&std::get<0>(*vertex));
    const auto* p1 = reinterpret_cast<const char*>(&std::get<Index>(*vertex));
    return static_cast<GLintptr>(p1 - p0);
}


template <int Index, class Vertex>
struct GetVertexAttributeTraits_ {
    static std::vector<VertexAttributeTraits> apply(const std::initializer_list<const char*>& names) {
        auto result = GetVertexAttributeTraits_<Index - 1, Vertex>::apply(names);
        result.push_back({
                names.begin()[Index],
                VertexAttributeSize<std::tuple_element_t<Index, Vertex>   >::value,
                VertexAttributeType<std::tuple_element_t<Index, Vertex>>::value,
                getVertexAttributeOffset<Index, Vertex>()
        });
        return result;
    }
};


template <class Vertex>
struct GetVertexAttributeTraits_<0, Vertex> {
    static std::vector<VertexAttributeTraits> apply(const std::initializer_list<const char*>& names) {
        return {{
                names.begin()[0],
                VertexAttributeSize<std::tuple_element_t<0, Vertex>>::value,
                VertexAttributeType<std::tuple_element_t<0, Vertex>>::value,
                getVertexAttributeOffset<0, Vertex>()
        }};
    }
};


template <class Vertex>
static std::vector<VertexAttributeTraits> getVertexAttributeTraits(const std::initializer_list<const char*>& names) {
    static_assert(std::tuple_size_v<Vertex> != 0, "incorrect number of names");
    assert(std::tuple_size_v<Vertex> == names.size()); // "incorrect number of names"
    return GetVertexAttributeTraits_<std::tuple_size_v<Vertex> - 1, Vertex>::apply(names);
}

namespace VertexAttributeAlias {
    using _1f = float;
    using _2f = glm::vec2;
    using _3f = glm::vec3;
    using _4f = glm::vec4;
}

using namespace VertexAttributeAlias;

template<typename... Types>
using Vertex = std::tuple<Types...>;


#endif
